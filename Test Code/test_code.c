#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define BUFFER_LENGTH 256
static char data[BUFFER_LENGTH];

int main(int argc, char* argv[]) {
  int ret, fd, i, n;

  printf("test started\n");

  fd = open("/dev/first_come_first_served", O_RDWR);

  if (fd < 0) {
    perror("open failed");
      return errno;
  }

  ret = write(fd, "12,12", BUFFER_LENGTH);

  if (ret < 0) {
    perror("write failed");
    return errno;
  }

  return 0;
}

