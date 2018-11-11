#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#define  DEVICE_NAME "sdf"
#define  CLASS_NAME  "myclass"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Adrianna Chang & Britta Evans");
MODULE_DESCRIPTION("Linux device to simulate shortest distance first elevator system");

static struct task_struct *elevator_thread;

// Elevator macros
#define NUM_FLOORS 6
#define ELEVATOR_CAPACITY 16

/* Elevator data structures */
typedef struct passengerNode {
  int id;
  int destination;
  struct passengerNode* next;
} passengerNode;

typedef struct floorQueue {
  int id;
  passengerNode* startQueue;
  passengerNode* endQueue;
} floorQueue;

typedef struct elevator {
  floorQueue* current_floor;
  int passengerCount;
  // array of passengerNode pointers; each array element represents a queue of passengers for a given floor,
  //with the first passenger in the queue being the one with the lowest priority id
  passengerNode* passengerArray[NUM_FLOORS];
} elevator;

/* Elevator global variables */
int static nextId = 1;

floorQueue shaftArray[NUM_FLOORS];
elevator elevatorCar;

int queueCount = 0;
int firstOrigin = -1; // To know when the first system call happens and the elevator needs to start moving

void getCurrentTime(struct timeval, unsigned long*, unsigned long*);

// elevator function prototypes
void initializeShaftArray(void);
void initializeElevatorCar(void);
int addPassengertoQueue(int, int);
int elevatorUp(void);
int elevatorDown(void);
void pickUp(void);
void enterElevator(passengerNode*);
void dropOff(void);
int existsPassengerNode(void);

// thread function prototypes
int thread_fn(void*);
int thread_init(void);
void thread_cleanup(void);

// data from userspace
//static char message[256] = {0};

//Automatically determined device number
static int majorNumber;

//Driver class struct ptr
static struct class *driverClass = NULL;

//Driver device struct ptr
static struct device *driverDevice = NULL;

//Driver prototype functions
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/* Driver-operation associations
 */
static struct file_operations fops = {
  .open = dev_open,
  .read = dev_read,
  .write = dev_write,
  .release = dev_release,
};

/* Initialization function
 */
static int __init sdf_init(void) {
  printk(KERN_INFO "sdf: initializing\n");

  // dynamically allocate a major number
  majorNumber = register_chrdev(0, DEVICE_NAME, &fops);

  if (majorNumber<0) {
    printk(KERN_ALERT "sdf: failed to allocate major number\n");
    return majorNumber;
  }

  printk(KERN_INFO "sdf: registered with major number %d\n", majorNumber);

  // register device class
  driverClass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(driverClass)) {
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "sdf: failed to register device class\n");
    return PTR_ERR(driverClass);
  }

  printk(KERN_INFO "sdf: device class registered\n");

  // register device driver
  driverDevice = device_create(driverClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
  if (IS_ERR(driverDevice)) {
    // if error, cClean up
    class_destroy(driverClass); unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "sdf: failed to create device\n");
    return PTR_ERR(driverDevice);
  }

  initializeShaftArray();
  initializeElevatorCar();

  printk(KERN_INFO "sdf: device class created\n");

  thread_init();

  return 0;
}

static void __exit sdf_exit(void) {
  thread_cleanup();
  // remove device
  device_destroy(driverClass, MKDEV(majorNumber, 0));
  // unregister device class
  class_unregister(driverClass);
  // remove device class
  class_destroy(driverClass);
  // unregister major number
  unregister_chrdev(majorNumber, DEVICE_NAME);
  printk(KERN_INFO "sdf: closed\n");
}

/* Called each time the device is opened.
 * inodep = pointer to inode
 * filep = pointer to file object
*/

static int dev_open(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "sdf: opened\n");
  return 0;
}

/* Called when device is read.
* filep = pointer to a file
* buffer = pointer to the buffer to which this function writes the data
* len = length of buffer
* offset = offset in buffer
*/
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
  return 0;
}

/* Called whenever device is written.
* filep = pointer to file
* buffer = pointer to buffer that contains data to write to the device
* len = length of data
* offset = offset in buffer
*/

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
  int origin, destination;
  char number[2] = {0};
  int bufferIndex = 0, numberIndex = 0;

  // append received message
  // sprintf(message, "%s = %d bytes", buffer, (int)len);
  //printk(KERN_INFO "sdf: received %d characters from the user\n", (int)len);
  // printk(KERN_INFO "message: %s\n", message);

  while (buffer[bufferIndex] != 0) {
    if (buffer[bufferIndex] == ',') {
      numberIndex = 0;
      sscanf(number, "%d", &origin);
      memset(&number[0], 0, sizeof(number)); // Clear the number buffer
    } else {
      number[numberIndex] = buffer[bufferIndex]; //add to char array
      numberIndex++;
    }
    bufferIndex++;
  }
  sscanf(number, "%d", &destination);

  addPassengertoQueue(origin, destination);

  if (firstOrigin < 0) {
    firstOrigin = origin;
  }
  queueCount++;


  return len;
}

/* Called when device is closed/released. * inodep = pointer to inode
* filep = pointer to a file
*/
static int dev_release(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "sdf: released\n");
  return 0;
}

void getCurrentTime(struct timeval tv, long unsigned *sec, long unsigned *usec) {
  do_gettimeofday(&tv);

  *sec = tv.tv_sec;
  *usec = tv.tv_usec;
}

void initializeShaftArray() {
  int i;

  printk(KERN_INFO "Initializing shaft array!\n");

  for(i=0; i<NUM_FLOORS; i++) {
    passengerNode* startQueue = NULL;
    passengerNode* endQueue = NULL;
    floorQueue new_floor = { i, startQueue, endQueue };
    shaftArray[i] = new_floor;
  }
}

void initializeElevatorCar() {
  int i;
  passengerNode* init_array[NUM_FLOORS] = {0};

  for(i=0; i<NUM_FLOORS; i++) {
    passengerNode* ptr = NULL;
    init_array[i] = ptr;
  }

  elevatorCar.current_floor = &shaftArray[0];
  elevatorCar.passengerCount = 0;
  memcpy(elevatorCar.passengerArray, init_array, sizeof(elevatorCar.passengerArray));
}

int addPassengertoQueue(int origin, int destination) {
  passengerNode* new_passenger;

  if (origin < 0 || origin > NUM_FLOORS || destination < 0 || destination > NUM_FLOORS)
  {
    return -1;
  }

  if ((new_passenger = (passengerNode*) kmalloc(sizeof(*new_passenger), GFP_KERNEL)) == NULL) {
    return -1;
  }

  new_passenger->id = nextId++;
  new_passenger->destination = destination;
  new_passenger->next = NULL;

  if (shaftArray[origin].startQueue == NULL) {
    shaftArray[origin].startQueue = new_passenger;
    printk(KERN_INFO "Passenger id %d, floor %d -> floor %d", new_passenger->id, origin, destination);
  }

  else {
    shaftArray[origin].endQueue->next = new_passenger;
    printk(KERN_INFO "Passenger id %d, floor %d -> floor %d", new_passenger->id, origin, destination);
  }

  shaftArray[origin].endQueue = new_passenger;

  return 0;
}

int elevatorUp(){
  int current_floor;
  current_floor = elevatorCar.current_floor->id;
  if ( current_floor == NUM_FLOORS - 1 ) {
    return -1;
  }
  else {
    elevatorCar.current_floor = &shaftArray[++current_floor];
    msleep(1000);
  }
  return 0;
}

int elevatorDown(){
  int current_floor = elevatorCar.current_floor->id;
  if ( current_floor == 0 ) {
    return -1;
  }
  else {
    elevatorCar.current_floor = &shaftArray[--current_floor];
    msleep(1000);
  }
  return 0;
}

void pickUp() {
  int i;
  int delta = ELEVATOR_CAPACITY - elevatorCar.passengerCount;

  passengerNode* current_passenger, *next_passenger;
  current_passenger = elevatorCar.current_floor->startQueue;

  if (delta > 0) {
    for (i=0; i<delta; i++) {
      next_passenger = current_passenger->next;
      enterElevator(current_passenger);
      elevatorCar.passengerCount++;
      printk(KERN_INFO "Picking up passenger %d, passenger count = %d", current_passenger->id, elevatorCar.passengerCount);
      queueCount--;

      current_passenger = next_passenger;
      if (current_passenger == NULL) {
        break;
      }
    }
  }
  else {
    printk("Elevator full!");
  }

  msleep(1000);
}

void dropOff() {
  int current_floor = elevatorCar.current_floor->id;
  passengerNode *head = elevatorCar.passengerArray[current_floor];
  passengerNode *next_node;

  while (head != NULL) {
    elevatorCar.passengerCount--;
    printk(KERN_INFO "Dropping off passenger %d, passenger count = %d", head->id, elevatorCar.passengerCount);
    next_node = head->next;
    kfree(head);
    head = next_node;
  }
  elevatorCar.passengerArray[current_floor] = NULL;
  msleep(1000);
}

void enterElevator(passengerNode* entering_passenger) {
  int dest = entering_passenger->destination;
  elevatorCar.current_floor->startQueue = entering_passenger->next;

  if (elevatorCar.passengerArray[dest] == NULL) {
    elevatorCar.passengerArray[dest] = entering_passenger;
    entering_passenger->next = NULL;
  }
  else {
    if (elevatorCar.passengerArray[dest]->id > entering_passenger->id) {
      entering_passenger->next =  elevatorCar.passengerArray[dest];
      elevatorCar.passengerArray[dest] = entering_passenger;
    }
    else {
      entering_passenger->next =  elevatorCar.passengerArray[dest]->next;
      elevatorCar.passengerArray[dest]->next = entering_passenger;

    }
  }

  if (elevatorCar.current_floor->startQueue == NULL) {
    elevatorCar.current_floor->endQueue = NULL;
  }
}

int existsPassengerNode(){
  if (queueCount == 0 && elevatorCar.passengerCount == 0)
    return 0;
  return 1;
}

int checkPriorityInElevator(int floor_num) {
  if (floor_num < 0 || floor_num > NUM_FLOORS - 1) {
    return 0;
  }

  if (elevatorCar.passengerArray[floor_num] == NULL) {
    return 0;
  }
  else {
    return elevatorCar.passengerArray[floor_num]->id;
  }
}

int checkPriorityInShaft(int floor_num) {
  if (floor_num < 0 || floor_num > NUM_FLOORS - 1) {
    return 0;
  }

  if (shaftArray[floor_num].startQueue == NULL) {
    return 0;
  }
  else {
    return shaftArray[floor_num].startQueue->id;
  }
}

int thread_fn(void * v) {
  /* Structures for calculating time */
  struct timeval tv;
  unsigned long start_sec, start_usec, end_sec, end_usec, total_sec, total_usec;
  char buffer[256];

  int counter = 0; //Ensure we don't get stuck in loop if passengers aren't being created properly
  // First pickUp variables
  int dist_to_origin;
  int i, j;
  int floor_up_priority, floor_down_priority;

  // Main loop to run elevator
  while (firstOrigin < 0 && counter < 600000){
    msleep(1000);
    counter++;
  }

  if (firstOrigin < 0) {
    return 0;
  }

  //Start time
  printk(KERN_INFO "Start time: ");
  getCurrentTime(tv, &start_sec, &start_usec);
  sprintf(buffer, "%lu sec %lu usec\n", start_sec, start_usec);
  printk(KERN_INFO "%s", buffer);

  dist_to_origin = firstOrigin - elevatorCar.current_floor->id;

  for(i=0; i<dist_to_origin; i++) {
    int current_floor = elevatorCar.current_floor->id;
    if ( current_floor < NUM_FLOORS - 1 ) {
      elevatorCar.current_floor = &shaftArray[++current_floor];
      msleep(1000);
    }
  }

  pickUp();

  while(existsPassengerNode() > 0) {
    if(kthread_should_stop()) {
      do_exit(0);
    }
    // Algorithm
    // Check for pick up
    if (shaftArray[elevatorCar.current_floor->id].startQueue != NULL) {
      pickUp();
    }

    // Check if anyone in elevator
    // If no, then we need to check floors
    if (elevatorCar.passengerCount == 0) {
      // Check for closest floor with passenger to move to
      for(i=1; i<NUM_FLOORS; i++) {
        floor_up_priority = checkPriorityInShaft((elevatorCar.current_floor->id) + i);
        floor_down_priority = checkPriorityInShaft((elevatorCar.current_floor->id) - i);

        // printk(KERN_INFO "floor_up_priority: %d", floor_up_priority);
        // printk(KERN_INFO "floor_down_priority: %d", floor_down_priority);
        // printk(KERN_INFO "floor up: %d", elevatorCar.current_floor->id + i);
        // printk(KERN_INFO "floor down: %d", elevatorCar.current_floor->id - i);

         // If there is only a passenger to be picked up on a floor above elevatorCar's current pos
        if (floor_up_priority != 0 && floor_down_priority == 0) {
          for (j = 0; j<i; j++){
            elevatorUp();
            printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
          }
          break;
        }
        // If there is only a passenger to be picked up on a floor below elevatorCar's current pos
        else if (floor_up_priority == 0 && floor_down_priority != 0) {
          for (j = 0; j<i; j++){
            elevatorDown();
            printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
          }
          break;
        }
        // If we have passengers to be picked up on floors of equal distance from elevatorCar's current position,
        // we need to compare ids to determine priority
        else if (floor_up_priority != 0 && floor_down_priority != 0) {
          if (floor_up_priority < floor_down_priority) {
            for (j = 0; j<i; j++){
              elevatorUp();
              printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
            }
          }
          else {
            for (j = 0; j<i; j++){
              elevatorDown();
              printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
            }
          }
          break;
        }
      }
      pickUp();
    }

    //Finding closest floor for drop off
    // Same logic as above, but checking the elevatorCar rather than the floors
    // In order to determine drop off
    for(i=1; i<NUM_FLOORS; i++) {
      floor_up_priority = checkPriorityInElevator((elevatorCar.current_floor->id) + i);
      floor_down_priority = checkPriorityInElevator((elevatorCar.current_floor->id) - i);

      if (floor_up_priority != 0 && floor_down_priority == 0) {
        for (j = 0; j<i; j++){
          elevatorUp();
          printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
        }
        break;
      }
      else if (floor_up_priority == 0 && floor_down_priority != 0) {
        for (j = 0; j<i; j++){
          elevatorDown();
          printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
        }
        break;
      }
      else if (floor_up_priority != 0 && floor_down_priority != 0) {
        if (floor_up_priority < floor_down_priority) {
          for (j = 0; j<i; j++){
            elevatorUp();
            printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
          }
        }
        else {
          for (j = 0; j<i; j++){
            elevatorDown();
            printk(KERN_INFO "------------------------------------------Floor: %d", elevatorCar.current_floor->id);
          }
        }
        break;
      }
    }
    // Check for drop off
    if (elevatorCar.passengerArray[elevatorCar.current_floor->id] != NULL) {
      dropOff();
    }
  }

  //print results!
  printk(KERN_INFO "---- SHORTEST DISTANCE FIRST ALGORITHM COMPLETE ----");
  printk(KERN_INFO "Start time: ");
  sprintf(buffer, "%lu sec %lu usec\n", start_sec, start_usec);
  printk(KERN_INFO "%s", buffer);
  printk(KERN_INFO "End time: ");
  getCurrentTime(tv, &end_sec, &end_usec);
  sprintf(buffer, "%lu sec %lu usec\n", end_sec, end_usec);
  printk(KERN_INFO "%s", buffer);

  if (end_usec < start_usec) {
    total_sec = end_sec - 1 - start_sec;
    total_usec = end_usec + 1000000 - start_usec;
  }
  else {
    total_sec = end_sec - start_sec;
    total_usec = end_usec - start_usec;
  }

  printk(KERN_INFO "Total sec: %lu, Total: usec: %lu", total_sec, total_usec);
  printk(KERN_INFO "Done");

  return 0;
}


// From http://tuxthink.blogspot.ca/2014/06/teminating-kernel-thread-using.html
// (thread_init, thread_cleanup code)
int thread_init(void) {
    char our_thread[16]="elevator-thread";

    elevator_thread = kthread_create(thread_fn, NULL, our_thread);
    if((elevator_thread))
    {
      wake_up_process(elevator_thread);
    }

    return 0;
}

void thread_cleanup(void) {
  int ret;
  printk(KERN_INFO "cleanup...");
  printk(KERN_INFO "thread_state: %ld", elevator_thread->state);
  if (elevator_thread->state!= 2)
  {
    printk(KERN_INFO "Not stopping thread");
  }
  else {
    ret = kthread_stop(elevator_thread);
    printk("value of ret = %d", ret);
    if(ret == 0)
     printk(KERN_INFO "Thread stopped");
  }
}

module_init(sdf_init);
module_exit(sdf_exit);
