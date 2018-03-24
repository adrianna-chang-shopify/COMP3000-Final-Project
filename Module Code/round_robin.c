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

#define  DEVICE_NAME "round_robin"
#define  CLASS_NAME  "myclass"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Adrianna Chang & Britta Evans");
MODULE_DESCRIPTION("Linux device to simulate round robin elevator system");

static struct task_struct *elevator_thread;

// Elevator macros
#define NUM_FLOORS 6
#define ELEVATOR_CAPACITY 8

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
static char message[256] = {0};

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
static int __init round_robin_init(void) {
  printk(KERN_INFO "round_robin: initializing\n");

  // dynamically allocate a major number
  majorNumber = register_chrdev(0, DEVICE_NAME, &fops);

  if (majorNumber<0) {
    printk(KERN_ALERT "round_robin: failed to allocate major number\n");
    return majorNumber;
  }

  printk(KERN_INFO "round_robin: registered with major number %d\n", majorNumber);

  // register device class
  driverClass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(driverClass)) {
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "round_robin: failed to register device class\n");
    return PTR_ERR(driverClass);
  }

  printk(KERN_INFO "round_robin: device class registered\n");

  // register device driver
  driverDevice = device_create(driverClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
  if (IS_ERR(driverDevice)) {
    // if error, cClean up
    class_destroy(driverClass); unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "round_robin: failed to create device\n");
    return PTR_ERR(driverDevice);
  }

  initializeShaftArray();
  initializeElevatorCar();

  printk(KERN_INFO "round_robin: device class created\n");

  thread_init();

  return 0;
}

static void __exit round_robin_exit(void) {
  thread_cleanup();
  // remove device
  device_destroy(driverClass, MKDEV(majorNumber, 0));
  // unregister device class
  class_unregister(driverClass);
  // remove device class
  class_destroy(driverClass);
  // unregister major number
  unregister_chrdev(majorNumber, DEVICE_NAME);
  printk(KERN_INFO "round_robin: closed\n");
}

/* Called each time the device is opened.
 * inodep = pointer to inode
 * filep = pointer to file object
*/

static int dev_open(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "round_robin: opened\n");
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
  sprintf(message, "%s = %d bytes", buffer, (int)len);
  printk(KERN_INFO "round_robin: received %d characters from the user\n", (int)len);
  printk(KERN_INFO "message: %s\n", message);

  while (buffer[bufferIndex] != 0) {
    if (buffer[bufferIndex] == ',') {
      numberIndex = 0;
      sscanf(number, "%d", &origin);
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
  printk(KERN_INFO "round_robin: released\n");
  return 0;
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
    printk(KERN_INFO "origin: %d, startQueue id: %d", origin, shaftArray[origin].startQueue->id);
  }

  else {
    shaftArray[origin].endQueue->next = new_passenger;
    printk(KERN_INFO "origin: %d, endQueue id: %d", origin, shaftArray[origin].endQueue->id);
  }

  shaftArray[origin].endQueue = new_passenger;

  return 0;
}

int elevatorUp(){
  int current_floor = elevatorCar.current_floor->id;
  if ( current_floor == NUM_FLOORS - 1 ) {
    return -1;
  }
  else {
    elevatorCar.current_floor = &shaftArray[current_floor++];
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
    elevatorCar.current_floor = &shaftArray[current_floor--];
    msleep(1000);
  }
  return 0;
}

void pickUp() {
  int i;
  int delta = ELEVATOR_CAPACITY - elevatorCar.passengerCount;

  passengerNode* current_passenger = elevatorCar.current_floor->startQueue;

  for (i=0; i<delta; i++) {
    enterElevator(current_passenger);
    elevatorCar.passengerCount++;
    queueCount--;
    current_passenger = current_passenger->next;
    if (current_passenger == NULL) {
      break;
    }
  }
  msleep(1000);
}

void dropOff() {
  int current_floor = elevatorCar.current_floor->id;
  passengerNode *head = elevatorCar.passengerArray[current_floor];
  passengerNode *next_node;

  while (head != NULL) {
    elevatorCar.passengerCount--;
    next_node = head->next;
    kfree(head);
    head = next_node;
  }
  msleep(1000);
}

void enterElevator(passengerNode* entering_passenger) {
  int dest = entering_passenger->destination;

  if (elevatorCar.passengerArray[dest] == NULL) {
    elevatorCar.passengerArray[dest] = entering_passenger;
    elevatorCar.current_floor->startQueue = entering_passenger->next;
    entering_passenger->next = NULL;
  }
  else {
    elevatorCar.current_floor->startQueue = entering_passenger->next;
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

int thread_fn(void * v) {
  int counter = 0; //Ensure we don't get stuck in loop if passengers aren't being created properly
  // First pickUp variables
  int dist_to_origin = firstOrigin - elevatorCar.current_floor->id;
  int i;
  int floor_to_check;
  int pick_up;
  int drop_off;

  int elevatorDirection = 1; // 1 = up, -1 = down

  // Main loop to run elevator
  printk(KERN_INFO "In elevator-thread");
  while (firstOrigin < 0 && counter < 10){
    printk(KERN_INFO "Waiting for passengers!\n");
    msleep(1000);
    counter++;
  }

  printk(KERN_INFO "FirstOrigin: %d\n", firstOrigin);

  // First pickUp
  for(i=0; i<dist_to_origin; i++) {
    elevatorUp();
  }
  pickUp();

  while(existsPassengerNode() > 0) {
    // Algorithm
    printk(KERN_INFO "Current floor: %d\n", elevatorCar.current_floor->id);
    if (elevatorCar.current_floor->id == 0) {
      printk(KERN_INFO "Changing direction to UP\n");
      elevatorDirection = 1;
    }
    else if (elevatorCar.current_floor->id == 6) {
      printk(KERN_INFO "Changing direction to DOWN\n");
      elevatorDirection = -1;
    }

    floor_to_check = elevatorCar.current_floor->id + elevatorDirection;
    pick_up = shaftArray[floor_to_check].startQueue != NULL;
    drop_off = elevatorCar.passengerArray[floor_to_check] != NULL;

    if ( drop_off || pick_up) {
      printk(KERN_INFO "Passengers to pick up or drop off\n");
      //Move
      if (elevatorDirection == 1) {
        elevatorUp();
      }
      else if (elevatorDirection == -1) {
        elevatorDown();
      }
      //drop off / pick up
      if (drop_off) {
        printk(KERN_INFO "Dropping off!\n");
        dropOff();
      }
      if (pick_up) {
        printk(KERN_INFO "Picking off!\n");
        pickUp();
      }
    }

    else if (floor_to_check == 0) {
      printk(KERN_INFO "Changing direction to UP\n");
      elevatorDirection = 1;
    }
    else if (floor_to_check == 6) {
      printk(KERN_INFO "Changing direction to DOWN\n");
      elevatorDirection = -1;
    }
    else {
      if (elevatorDirection == 1) {
        elevatorUp();
      }
      else if (elevatorDirection == -1) {
        elevatorDown();
      }
    }

    if (existsPassengerNode() == 0) {
      printk(KERN_INFO "Sleeping while waiting for passengers\n");
      msleep(1000);
    }
  }

  //print results!

  return 0;
}


// From http://tuxthink.blogspot.ca/2011/02/kernel-thread-creation-1.html
// (thread_init, thread_cleanup code)
int thread_init(void) {
    char our_thread[16]="elevator-thread";
    void* data = NULL;

    printk(KERN_INFO "in init");
    elevator_thread = kthread_create(thread_fn, data, our_thread);
    if((elevator_thread))
    {
      wake_up_process(elevator_thread);
    }

    return 0;
}

void thread_cleanup(void) {
  int ret;
  ret = kthread_stop(elevator_thread);
  if(!ret)
    printk(KERN_INFO "Thread stopped");
}

module_init(round_robin_init);
module_exit(round_robin_exit);
