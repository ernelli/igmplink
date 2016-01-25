#include<sys/types.h>

#include<sys/socket.h>
#include <netinet/in.h>

#include<sys/time.h>

#include<errno.h>
#include<stdio.h>
#include<string.h>

void hexdump(const unsigned char *data, int len) {
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

#define MAX_TIMERS 128

typedef int (*callback_t)(void *);

typedef long long timestamp;

struct timer_t {
timestamp time;
callback_t cb;
};

struct timer_t timers[MAX_TIMERS];


timestamp getCurrentTime() {
struct timeval time;
gettimeofday(&time, NULL);

return (timestamp)time.tv_sec*1000 + (timestamp)time.tv_usec/1000;
}

int setTimeout(callback_t cb, timestamp timeout) {

}


int main(int argc, char *argv[]) {
  unsigned char buffer[2048];
  int len;

  printf("Create raw socket listen to igmp: 0x%04x\n", IPPROTO_IGMP);

  printf("currTime: %Ld\n", getCurrentTime());
  sleep(1);
  printf("currTime: %Ld\n", getCurrentTime());

  int fd = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);

  if(fd > 0) {

    printf("got socket: %d\n", fd);

    


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
