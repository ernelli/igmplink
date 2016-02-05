#include<sys/types.h>
#include<sys/socket.h>
#include <netinet/in.h>
#include<sys/time.h>
#include<sys/select.h>

#include<unistd.h>
#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<memory.h>
#include<ctype.h>

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

typedef void (*callback_t)(void *);

typedef long long timestamp;

struct timer_t {
  timestamp time;
  callback_t cb;
  void *closure;
};

struct timer_t timers[MAX_TIMERS];
int num_timers = 0;

timestamp getCurrentTime() {
  struct timeval time;
  gettimeofday(&time, NULL);
  
  return (timestamp)time.tv_sec*1000 + (timestamp)time.tv_usec/1000;
}

int setTimeout(callback_t cb, timestamp timeout, void *closure) {
  if(num_timers >= MAX_TIMERS) {
    fprintf(stderr, "setTimeout failed, MAX_TIMERS: %d used\n", MAX_TIMERS);
    exit(1);
  }

  if(timeout < 0) {
    timeout = 0;
  }
  
  timestamp now = getCurrentTime();
  
  timestamp timeoutTime = now + timeout;
  int i = 0;
  while(i < num_timers && timers[i].time <= timeoutTime) {
    i++;
  }
  
  //insert timer
  memmove(timers+i+1, timers+i, sizeof(timers[0])*(num_timers-i));
  timers[i].time = timeoutTime;
  timers[i].cb = cb;
  timers[i].closure = closure;
  num_timers++;
  
  
  return i;
}

void _clearTimeout(int index) {
  //remove timer
  memmove(timers+index, timers+index+1, sizeof(timers[0])*(num_timers-index));
  num_timers--;
}

timestamp _nextTimeout() {
  timestamp next = -1;
  
  if(num_timers > 0) {
    next = timers[0].time - getCurrentTime();
    
    if(next < 0) {
      next = 0;
    }
  } 
  return next;
}


void _dumpTimers() {
  printf("num timers: %d\n", num_timers);
  for(int i = 0; i < num_timers; i++) {
    printf("timer: %d, timeout: %Ld, callback: %016Lx\n", i, timers[i].time, (long long)timers[i].cb);
  }
}


int main(int argc, char *argv[]) {
  unsigned char buffer[2048];
  timestamp _dt;
  int len;
  int run = 1;


  printf("Create raw socket listen to igmp: 0x%04x\n", IPPROTO_IGMP);

  void cb(void *c) { 
    printf("timer %s kicked\n", (const char*)c); 
  }

  setTimeout(cb, 400, "second");
  setTimeout(cb, 300, "first");
  setTimeout(cb, 900, "last");


  int fd = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
  printf("got socket: %d\n", fd);

  if(fd > 0) {
    
    fd_set rfds;
    fd_set wfds;
    fd_set efds;

    FD_ZERO(&rfds);

    FD_SET(fd, &rfds);

    while(run) {
      _dt = _nextTimeout();
    
      struct timeval tv;      
      
      if(_dt > 0) {
        tv.tv_sec = _dt / 1000;
        tv.tv_usec = 1000*(_dt % 1000);
      } 

      printf("select with timeout: %d %d\n", tv.tv_sec, tv.tv_usec);


      FD_ZERO(&wfds);
      FD_ZERO(&efds);



      int retval = select(fd+1, &rfds, &wfds, &efds, _dt > 0 ? &tv : NULL);

      if(retval == -1) {
        perror("select()");
        exit(1);
      }

      printf("select returned: %d\n", retval);

      if(FD_ISSET(fd,&rfds)) {
        len = recv(fd, buffer, sizeof(buffer), 0);
        if(len > 0) {
          printf("got igmp packet:\n");
          hexdump(buffer, len);
        } 
        FD_CLR(fd, &rfds);
      }

      // invoke expired timers
      while(_nextTimeout() == 0) {
        timers[0].cb(timers[0].closure);
        _clearTimeout(0);
      }
      
    }
  } else {
    perror("Failed to create socket\n");
  }
  return 0;
}
