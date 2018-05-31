#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include <unistd.h>
#include<errno.h>
#include<limits.h>
#include<time.h>

#define BUFFER_LENGTH 256
#define NUM_PASSENGERS 30
#define NUM_FLOORS 6

static char data[BUFFER_LENGTH];

int main(int argc, char* argv[]) {
  int ret, fd, i, n;

  printf("test started\n");

  fd = open("/dev/sdf", O_RDWR);

  if (fd < 0) {
    perror("open failed");
      return errno;
  }


  time_t t;
  srand((unsigned) time(&t));
  int start, dest;

  for (i=0; i<NUM_PASSENGERS; i++) {
    // 50% of passengers will have completely random starting and ending floors
    if ((rand() % 2) == 0) {
      start = (rand() % (NUM_FLOORS - 1)) + 1;
      do {
        dest = (rand() % (NUM_FLOORS - 1)) + 1;
      } while (start == dest);
    }
    // 50% of passengers will either start from ground or have destination as ground
    else {
      // Half start from ground
      if ((rand() % 2) == 0) {
        start = 0;
        do {
          dest = rand() % NUM_FLOORS;
        } while (start == dest);
      }
      // Half end on ground
      else {
        dest = 0;
        do {
          start = rand() % NUM_FLOORS;
        } while (start == dest);
      }
    }

    sprintf(data, "%d,%d", start, dest);
    printf("Data: %s, id: %d\n", data, i+1);
    ret = write(fd, data, BUFFER_LENGTH);
    if (ret < 0) {
      perror("write failed");
      return errno;
    }
    sleep(2);
  }

/*
write(fd, "0,4", BUFFER_LENGTH);
sleep(2);
write(fd, "0,2", BUFFER_LENGTH);
sleep(2);
write(fd, "1,2", BUFFER_LENGTH);
sleep(2);
write(fd, "0,1", BUFFER_LENGTH);
sleep(2);
write(fd, "0,4", BUFFER_LENGTH);
sleep(2);
write(fd, "1,5", BUFFER_LENGTH);
sleep(2);
write(fd, "2,4", BUFFER_LENGTH);
sleep(2);
write(fd, "1,4", BUFFER_LENGTH);
sleep(2);
write(fd, "3,5", BUFFER_LENGTH);
sleep(2);
write(fd, "1,2", BUFFER_LENGTH);
sleep(2);
write(fd, "3,4", BUFFER_LENGTH);
sleep(2);
write(fd, "0,1", BUFFER_LENGTH);
sleep(2);
write(fd, "5,0", BUFFER_LENGTH);
sleep(2);
write(fd, "1,4", BUFFER_LENGTH);
sleep(2);
write(fd, "0,3", BUFFER_LENGTH);
sleep(2);
write(fd, "3,0", BUFFER_LENGTH);
sleep(2);
write(fd, "5,0", BUFFER_LENGTH);
sleep(2);
write(fd, "5,2", BUFFER_LENGTH);
sleep(2);
write(fd, "4,2", BUFFER_LENGTH);
sleep(2);
write(fd, "5,2", BUFFER_LENGTH);
sleep(2);
write(fd, "5,4", BUFFER_LENGTH);
sleep(2);
write(fd, "1,0", BUFFER_LENGTH);
sleep(2);
write(fd, "3,1", BUFFER_LENGTH);
sleep(2);
write(fd, "3,4", BUFFER_LENGTH);
sleep(2);
write(fd, "2,0", BUFFER_LENGTH);
sleep(2);
write(fd, "2,5", BUFFER_LENGTH);
sleep(2);
write(fd, "5,0", BUFFER_LENGTH);
sleep(2);
write(fd, "2,1", BUFFER_LENGTH);
sleep(2);
write(fd, "0,3", BUFFER_LENGTH);
sleep(2);
write(fd, "2,0", BUFFER_LENGTH);
sleep(2);
*/
  return 0;
}
