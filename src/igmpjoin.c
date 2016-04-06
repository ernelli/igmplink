#include<sys/types.h>
#include<sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>


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

int main(int argc, char *argv[]) {
  unsigned char buffer[2048];
  int port = 5555;
  char *address = "239.16.16.1";
  char *ifname = "eth0";
  int len;
  struct sockaddr_in mcaddr;
  int fd;

  memset(&mcaddr, 0, sizeof(mcaddr));
  mcaddr.sin_family = AF_INET;
  mcaddr.sin_port = htons(port);
  if(inet_pton(AF_INET, address, &mcaddr.sin_addr) <= 0) {
    fprintf(stderr, "Invalid multicast address: %s, error: %s\n", address, strerror(errno));
    return 1;
  }

  fd  = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd == -1) {
    fprintf(stderr, "Failed to create  multicast socket for address: %s, error: %s\n", address, strerror(errno));
    return 1;
  }
  
  if(fd > 0) {
    printf("got socket: %d\n", fd);

    bind(fd, (struct sockaddr *)&mcaddr, sizeof(mcaddr));    

#ifdef MCAST_JOIN_GROUP
    socklen_t addrlen;
    struct group_req greq;
    
    addrlen = sizeof(struct sockaddr_in);
    
    memset(&greq, 0, sizeof(greq));
    memcpy(&greq.gr_group, &mcaddr, addrlen);
    greq.gr_interface = if_nametoindex(ifname);
    if (setsockopt(fd, IPPROTO_IP, MCAST_JOIN_GROUP, (char *)&greq, sizeof(greq)) < 0) {
      fprintf(stderr, "Failed to join gruop: %s\n", strerror(errno));
      return 1;
    }

    printf("group joined, wait for data\n");

#else
    /*
    struct ip_mreq mreq;    

    memset(&mreq, 0, sizeof(mreq));
    memcpy(&mreq.imr_multiaddr, &mcaddr, sizeof(mreq.imr_multiaddr));
    setsockopt (fd, IPPROTO_IP, MCAST_JOIN_GROUP, &mreq, sizeof(mreq));    
    */
#endif
    while(1) {
      printf("calling recv on socket\n");

      len = recv(fd, buffer, sizeof(buffer), 0);
      
      if(len > 0) {
        printf("got igmp packet:\n");
        hexdump(buffer, len);
      } else {
        printf("recv returned, len: %d\n", len);
        return 1;
      }
    }
  } else {
    fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
  }
  return 0;
}
