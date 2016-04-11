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
  
  printf("got socket: %d\n", fd);
  
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

  printf("group joined, wait for data\n");

  return fd;
}

#define MAX_STREAMS 256
#define MAX_PIDS 8

struct pid_t {
  int pid;
  int cc;
  int packets;
  int bytes;
};

struct stream_t {
  char address[64]; 
  int fd;
  
  struct pid_t pids[MAX_PIDS];
};

struct stream_t streams[MAX_STREAMS];
int num_streams = 0;


int addPacket(struct stream_t *s, unsigned char *packet) {

  return 0;
}

struct stream_t *addStream(int fd) {
  struct stream_t *s = NULL;

  while(num_streams < MAX_STREAMS) {
    s = &streams[num_streams++];
    memset(s, 0, sizeof(struct stream_t));
    s->fd = fd;
  }

  return s;
}

int setupSelect(fd_set *rfds, fd_set *efds) {
  int i, maxfd = 0;
  FD_ZERO(rfds);
  FD_ZERO(efds);

  for(i = 0; i < num_streams; i++) {
    if(streams[i].fd > maxfd) {
      maxfd = streams[i].fd;
    }
    FD_SET(streams[i].fd, rfds);
    FD_SET(streams[i].fd, efds);
  }
  return maxfd;
}

int doSelect(int maxfd, fd_set *rfds, fd_set *wfds, fd_set *efds, int timeout) {
  struct timeval _timeout;
  memset(&_timeout, 0, sizeof(struct timeval));
  _timeout.tv_sec = timeout / 1000;
  _timeout.tv_usec = 1000*(timeout % 1000);

  int ret = select(1+maxfd, rfds, wfds, efds, &_timeout);

  if(ret == 0) {
    printf("Select timeout...\n");
  }

  return ret;
}


int main(int argc, char *argv[]) {
  unsigned char buffer[2048];
  int port = 5555;
  const char *address = "239.16.16.202";
  const char *ifname = "eth0";

  int fd;

  int check_cc = 0;
  int verbose = 0;

  const char *strarg(narg) {
    if(narg < argc) {
      return argv[narg];
    }
    fprintf(stderr, "string expected\n");
    exit(1);
  }

  int intarg(narg) {
    int val;
    if(narg < argc) {
      if(sscanf(argv[narg], "%i", &val) == 1) {
        return val;
      }
    }
    fprintf(stderr, "integer expected: %s\n", argv[narg]);
    exit(1);
  }

  int narg = 1;
  int ndef = 0;

  while(narg < argc) {
    if(!strcmp("-i", argv[narg])) {
      narg++;
      ifname = strarg(narg);
    } else if(!strcmp("-c", argv[narg])) {
      check_cc = 1;
    } else if(!strcmp("-v", argv[narg])) {
      verbose = 1;
    } else {

      switch(ndef++) {
        case 0:
          address = strarg(narg);
          break;
          
        case 1:
          port = intarg(narg);
          break;

        default:
          fprintf(stderr, "Unknown argument: %s\n", argv[narg]);      
          exit(1);
      }
    }
    narg++;
  }


  fd_set rfds;
  fd_set efds;

  printf("start multicast stream: %s:%d\n", address, port);

  fd = startMulticastStream(address, port, ifname);
  
  addStream(fd);

  int len;

  long long num_bytes = 0;
  
  timestamp startTime = 0;
  

  //int continutyCounter;

  printf("calling recv on socket\n");

  while(1) {
    int i;

    if(doSelect(setupSelect(&rfds, &efds), &rfds, NULL, &efds, 1000)) {
      printf("data ready...X\n");
      for(i = 0; i < num_streams; i++) {
        if(FD_ISSET(streams[i].fd, &rfds)) {
          len = recv(streams[i].fd, buffer, sizeof(buffer), 0);
          printf("got %d bytes on socket fd: %d\n", len, streams[i].fd);
        } else {
          printf("skip socket: %d\n", i);
        }
      }
      printf("Socket loop done");
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
