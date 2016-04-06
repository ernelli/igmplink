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
  int port = 5055;
  char *address = "239.16.16.202";
  char *ifname = "eth1";
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
  int mcsocket = socket(AF_INET, SOCK_DGRAM, 0);
  if(mcsocket == -1) {
    fprintf(stderr, "Failed to create  multicast socket for address: %s, error: %s\n", address, strerror(errno));
    return 1;
  }

  if(fd > 0) {
    
    printf("got socket: %d\n", fd);

    struct ip_mreq mreqn;
    
    memset(&mreq, 0, sizeof(mreq));

    memcpy(&mreq.imr_multiaddr, mcaddr, sizeof(mreq.imr_multiaddr));

    setsockopt (fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));    
    
    while(1) {
      len = recv(fd, buffer, sizeof(buffer), 0);
      
      if(len > 0) {
        printf("got igmp packet:\n");
        hexdump(buffer, len);
      } 
    }
  } else {
    printf("Failed to create socket: %s\n", strerror(errno));
  }
  return 0;
}
