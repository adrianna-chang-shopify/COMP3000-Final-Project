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

  fd = open("/dev/round_robin", O_RDWR);

  if (fd < 0) {
    perror("open failed");
      return errno;
  }

  ret = write(fd, "0,4", BUFFER_LENGTH);

  if (ret < 0) {
    perror("write failed");
    return errno;
  }

  return 0;
}
