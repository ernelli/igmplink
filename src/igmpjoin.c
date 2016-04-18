#include<sys/types.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<sys/select.h>

#include<netinet/in.h>
#include<arpa/inet.h>
#include<net/if.h>

#include<stdlib.h>
#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<memory.h>

void 
hexdump(data, len) 
const unsigned char *data;
int len;
{
  int i;
  char line[60], *d;
  int m = 0;
  char ascii[32];

  d = line;
  
  d = line + sprintf(line, "0000: ");

  for(i = 0; i < len; i++) {
    m = i & 0xf;

    if(i) {
      if(m == 0) {
        ascii[16] = '\0';
        printf("%s %s\n", line, ascii);
        d = line + sprintf(line, "%04x: ", i);
      } else if(m == 8) {
        d += sprintf(d, "-");
      } else {
        d += sprintf(d, " ");
      }
    }    

    d += sprintf(d, "%02x", data[i]);

    if(isprint(data[i])) {
      ascii[m] = data[i];
    } else {
      ascii[m] = '.';
    }
  }
  ascii[m+1] = '\0';
  printf("%-53s %s\n", line, ascii);
}


typedef long long timestamp;

timestamp getCurrentTime() {
  struct timeval time;
  gettimeofday(&time, NULL);
  
  return (timestamp)time.tv_sec*1000 + (timestamp)time.tv_usec/1000;
}

int startMulticastStream(const char *address, int port, const char *ifname) { 
  struct sockaddr_in mcaddr;
  int fd;
 
  memset(&mcaddr, 0, sizeof(mcaddr));
  mcaddr.sin_family = AF_INET;
  mcaddr.sin_port = htons(port);
  if(inet_pton(AF_INET, address, &mcaddr.sin_addr) <= 0) {
    fprintf(stderr, "Invalid multicast address: %s, error: %s\n", address, strerror(errno));
    return -1;
  }

  fd  = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd == -1) {
    fprintf(stderr, "Failed to create socket for multicast address: %s, error: %s\n", address, strerror(errno));
    return -1;
  }
  
  //printf("got socket: %d\n", fd);
  
  bind(fd, (struct sockaddr *)&mcaddr, sizeof(mcaddr));    
  
  socklen_t addrlen;
  struct group_req greq;
  
  addrlen = sizeof(struct sockaddr_in);
  
  memset(&greq, 0, sizeof(greq));
  memcpy(&greq.gr_group, &mcaddr, addrlen);
  greq.gr_interface = if_nametoindex(ifname);
  if (setsockopt(fd, IPPROTO_IP, MCAST_JOIN_GROUP, (char *)&greq, sizeof(greq)) < 0) {
    fprintf(stderr, "Failed to join gruop: %s\n", strerror(errno));
    return -1;
  }

//  printf("group joined, wait for data\n");

  return fd;
}

#define MAX_STREAMS 256
#define MAX_PIDS 16

struct pid_t {
  int pid;
  int cc;
  int packets;
  int bytes;
};

struct stream_t {
  char address[64]; 
  int fd;
  long long num_total_bytes;
  int num_packets;
  int num_bytes;
  int num_cc_error;
  int num_invalid;
  int analysis_error;
  struct pid_t pids[MAX_PIDS];
};

struct stream_t streams[MAX_STREAMS];
int num_streams = 0;

timestamp start_time;

int check_cc = 0;
int verbose = 0;

int addPacket(struct stream_t *s, unsigned char *packet, int len) {
  s->num_packets++;
  s->num_total_bytes += len;
  s->num_bytes += len;

  if(check_cc && !s->analysis_error) {
    const unsigned char *msg = packet;
    const unsigned char *end = packet + len - 4;

    int pid, cc, afe;

    while(msg < end) {
      if(*msg == 0x47) {

        pid = (msg[1] & 0x1f) << 8 | (msg[2] & 0xff);        
        cc = msg[3] & 0xf;        
        afe = (msg[3] >> 4) & 0x3;        

        if(pid != 0) {
          int pid_index = 0;
          while(s->pids[pid_index].pid != pid && s->pids[pid_index].pid) {
            pid_index++;
            if(pid_index >= MAX_PIDS) {
              s->analysis_error = 1;
              fprintf(stderr, "Max number of PIDS reached for stream: %s\n", s->address);
              return 0;
            }
          }
          if(!s->pids[pid_index].pid) {
            if(verbose) {
              printf("new pid %04x detected on stream: %s\n", pid, s->address);
            }
            s->pids[pid_index].pid = pid;
          } else if(pid != 0x1fff && (afe & 0x1) ) { // NULL packets does not use cc, and non payload packets does not increment cc
            if( ((s->pids[pid_index].cc + 1) & 0xf) != cc) {
              if(verbose) {
                printf("Continuty counter error on pid %04x detected on stream: %s, expected: %d got %d\n", pid, s->address, (s->pids[pid_index].cc + 1) & 0xf, cc);
              }
              s->num_cc_error++;
            }
          }

          //          if(msg[50] == 0x49 && msg[51] == 0x63)  {
          //          } else {
            s->pids[pid_index].cc = cc;
          //          }
        }
      } else {
        s->num_invalid++;
      }
      msg += 188;
    }

  }
  return 0;
}

struct stream_t *addStream(int fd, const char *address) {
  struct stream_t *s = NULL;

  if(num_streams < MAX_STREAMS) {
    s = &streams[num_streams++];
    memset(s, 0, sizeof(struct stream_t));
    s->fd = fd;
    if(address) {
      strcpy(s->address, address);
    }
  }

  return s;
}

timestamp cc_reset_time = 0;

void printStat() {
  int i;

  timestamp now = getCurrentTime();
  
  if(!cc_reset_time) {
    cc_reset_time = now + 10*1000;
  }

  double time_elapsed = (now - start_time)/1000.0;

  long long int total_bytes = 0;
  int total_packets = 0;

  int total_cc_errors = 0;
  int error_streams = 0;

  //printf("\x1b[2J");
  if(!verbose) {
    printf("\x1b[H");
  }

  for(i = 0; i < num_streams; i++) {
    printf("%2d %-15s %.3lf Mbit", i, streams[i].address, (8.0*(double)streams[i].num_bytes/(double)1E6)/time_elapsed);
    total_bytes += streams[i].num_bytes;
    total_packets += streams[i].num_packets;
    streams[i].num_bytes = 0;
    streams[i].num_packets = 0;

    if(streams[i].num_cc_error) {
      printf(" cc errors: %d", streams[i].num_cc_error);
      total_cc_errors += streams[i].num_cc_error;
      error_streams++;

      if(now > cc_reset_time) {
        streams[i].num_cc_error = 0;
      }
    }
    printf("\x1b[K\n");
  }

  if(now > cc_reset_time) {
    cc_reset_time = now + 10*1000;
  }

  printf("Total bitrate: %.3lf Mbit\x1b[K\n", (8.0*(double)total_bytes/(double)1E6)/time_elapsed);
  printf("Packets per second: %d\x1b[K\n", (int)(total_packets/time_elapsed));
  printf("Total cc errors: %d\x1b[K\n", total_cc_errors);
  printf("Number of erroneous streams: %d\x1b[K\n", error_streams);
}

int setupSelect(fd_set *rfds, fd_set *efds) {
  int i, maxfd = 0;
  FD_ZERO(rfds);
//  FD_ZERO(efds);

  for(i = 0; i < num_streams; i++) {
    if(streams[i].fd > maxfd) {
      maxfd = streams[i].fd;
    }
    FD_SET(streams[i].fd, rfds);
//    FD_SET(streams[i].fd, efds);
  }
  return maxfd;
}

int doSelect(int maxfd, fd_set *rfds, fd_set *wfds, fd_set *efds, int timeout) {
  struct timeval _timeout;
  memset(&_timeout, 0, sizeof(struct timeval));
  _timeout.tv_sec = timeout / 1000;
  _timeout.tv_usec = 1000*(timeout % 1000);

  int ret = select(1+maxfd, rfds, wfds, efds, &_timeout);

//  if(ret == 0) {
//    printf("Select timeout...\n");
//  }

  return ret;
}


int doRecv(struct stream_t *s) {
  unsigned char buffer[2048];
  int len;
#ifdef RECV_MMSG
          
#else


          do {
            len = recv(s->fd, buffer, sizeof(buffer), MSG_DONTWAIT);
            if(len > 0 ) {
              addPacket(s, buffer, len);
            } else {
              return -1;
            }
          } while(len > 0);
#endif
          return 0;
}

int main(int argc, char *argv[]) {

  int port = 5555;
  int running_time = 0;
  timestamp stop_time = 0;

//  const char *address = "239.16.16.202";
  const char *ifname = "eth0";

  const char *strarg(narg) {
    if(narg < argc) {
      return argv[narg];
    }
    fprintf(stderr, "string expected for %s\n", argv[narg-1]);
    exit(1);
  }

  int intarg(narg) {
    int val;
    if(narg < argc) {
      if(sscanf(argv[narg], "%i", &val) == 1) {
        return val;
      }
    }
    fprintf(stderr, "integer expected for %s\n", argv[narg-1]);
    exit(1);
  }

  int narg = 1;
//  int ndef = 0;

  while(narg < argc) {
    if(!strcmp("-i", argv[narg])) {
      narg++;
      ifname = strarg(narg);
    } else if(!strcmp("-p", argv[narg])) {
      port = intarg(narg);
    } else if(!strcmp("-c", argv[narg])) {
      check_cc = 1;
    } else if(!strcmp("-v", argv[narg])) {
      verbose = 1;
    } else if(!strcmp("-t", argv[narg])) {
      narg++;
      running_time = intarg(narg);
    } else if(!strcmp("-f", argv[narg])) {
      narg++;
      FILE *fp = fopen(strarg(narg), "r");
      if(fp) {
        while(!feof(fp)) {
          char addr[40];
          if(fscanf(fp, "%40s", addr) == 1) {
            printf("%2d %s\n", num_streams, addr);
            addStream(startMulticastStream(addr, port, ifname), addr);
          }
        }
      } else {
        fprintf(stderr, "Failed to open file: %s\n%s", argv[narg], strerror(errno));
      }
    } else {

      printf("%2d %s\n", num_streams, argv[narg]);
      addStream(startMulticastStream(argv[narg], port, ifname), argv[narg]);
    }
    narg++;
  }


  fd_set rfds;
  fd_set efds;

  fd_set _ref_fds;

  //printf("sizeof fdset: %d\n", sizeof(rfds));

  //printf("start multicast stream: %s:%d\n", address, port);
/*
  fd = startMulticastStream(address, port, ifname);
  addStream(fd, address);

  fd = startMulticastStream("239.16.16.232", port, ifname);
  addStream(fd, address);
*/

  if(!num_streams) {
    printf("no streams started\n");
    exit(1);
  }

  //int continutyCounter;

  start_time = getCurrentTime();

  if(running_time) {
    stop_time = start_time + 1000*running_time;
  }

  int max_fds = setupSelect(&_ref_fds, NULL);

  timestamp dt_pre = 0;
  //timestamp dt_select = 0;
  timestamp dt_recv = 0;

  printf("\x1b[2J");

  while(1) {
    int i, num_ready, delay;
    timestamp now;

    now = getCurrentTime();

    if(stop_time) {
      if(now >= stop_time) {

//        printf("time spend in dt_pre: %lld\n", dt_pre);
//        printf("time spend in dt_rect: %lld\n", dt_recv);
        
        return 0;
      }
    }

    delay = (int)(start_time + 1000 - now);

    if(delay <= 0) {
      printStat();
      start_time = now;
    }

    //printf("doSelect, timeout: %d\n", delay);

    memcpy(&rfds, &_ref_fds, sizeof(rfds));
    memcpy(&efds, &_ref_fds, sizeof(rfds));


    dt_pre += now - getCurrentTime();

    if( (num_ready=doSelect(max_fds, &rfds, NULL, NULL, delay)) > 0) {
      //printf("data ready, check %d streams\n", num_streams);


      now = getCurrentTime();

      for(i = 0; i < num_streams; i++) {
        if(FD_ISSET(streams[i].fd, &rfds)) {
          num_ready--;

          doRecv(&streams[i]);

        } else {
          //printf("skip socket: %d\n", i);
        }
        if(num_ready <= 0) {
          //printf("all streams checked, exit loop\n");
          break;
        }
      }

      dt_recv += getCurrentTime() - now;
      
#if 0     
      if(num_ready) {
        printf("check error fds, num_streams: %d\n", num_streams);
        for(i = 0; i < num_streams; i++) {
          if(FD_ISSET(streams[i].fd, &efds)) {
            num_ready--;
            printf("error on socket, fd: %d\n", streams[i].fd);
          } else {
            printf("skip socket: %d\n", i);
          }
          if(num_ready <= 0) {
            printf("all streams checked for errors, exit loop\n");
            break;
          }
        }
      }
#endif
    }
    
/*
      if(!startTime) {
        startTime = getCurrentTime();
      }
      
      if(len > 0) {
        if(verbose) {
          printf("got igmp packet:\n");
          hexdump(buffer, len);
        }

        if(!num_bytes ) {
          printf("Data received:\n");          
        }

        num_bytes += len;
        if(num_bytes >= 1024*1024) {
          timestamp now = getCurrentTime();
          printf("Bitrate: %d\n", (int)( (num_bytes*8000 / (now - startTime)) ));
          num_bytes = 0;
          startTime = now;
        }
      } else {
        printf("recv returned, len: %d\n", len);
        return 1;
      }
*/
    }

  return 0;
}
