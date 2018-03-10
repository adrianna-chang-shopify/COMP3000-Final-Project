#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define  DEVICE_NAME "first_come_first_served"
#define  CLASS_NAME  "myclass"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Adrianna Chang & Britta Evans");
MODULE_DESCRIPTION("Linux device to simulate first come first served elevator system");

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

// elevator function prototypes
void initializeShaftArray(void);
void initializeElevatorCar(void);
int addPassengertoQueue(int, int);
int elevatorUp(void);
int elevatorDown(void);
void pickUp(void);
void enterElevator(passengerNode*);
void dropOff(void);

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
static int __init first_come_first_served_init(void) {
  printk(KERN_INFO "first_come_first_served: initializing\n");

  // dynamically allocate a major number
  majorNumber = register_chrdev(0, DEVICE_NAME, &fops);

  if (majorNumber<0) {
    printk(KERN_ALERT "first_come_first_served: failed to allocate major number\n");
    return majorNumber;
  }

  printk(KERN_INFO "first_come_first_served: registered with major number %d\n", majorNumber);

  // register device class
  driverClass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(driverClass)) {
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "first_come_first_served: failed to register device class\n");
    return PTR_ERR(driverClass);
  }

  printk(KERN_INFO "first_come_first_served: device class registered\n");

  // register device driver
  driverDevice = device_create(driverClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
  if (IS_ERR(driverDevice)) {
    // if error, cClean up
    class_destroy(driverClass); unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "first_come_first_served: failed to create device\n");
    return PTR_ERR(driverDevice);
  }

  initializeShaftArray();
  initializeElevatorCar();

  printk(KERN_INFO "first_come_first_served: device class created\n");

  return 0;
}

static void __exit first_come_first_served_exit(void) {
  // remove device
  device_destroy(driverClass, MKDEV(majorNumber, 0));
  // unregister device class
  class_unregister(driverClass);
  // remove device class
  class_destroy(driverClass);
  // unregister major number
  unregister_chrdev(majorNumber, DEVICE_NAME);
  printk(KERN_INFO "first_come_first_served: closed\n");
}

/* Called each time the device is opened.
 * inodep = pointer to inode
 * filep = pointer to file object
*/

static int dev_open(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "first_come_first_served: opened\n");
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
  printk(KERN_INFO "first_come_first_served: received %d characters from the user\n", (int)len);
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


  return len;
}

/* Called when device is closed/released. * inodep = pointer to inode
* filep = pointer to a file
*/
static int dev_release(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "first_come_first_served: released\n");
  return 0;
}

module_init(first_come_first_served_init);
module_exit(first_come_first_served_exit);

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
    current_passenger = current_passenger->next;
    if (current_passenger == NULL) {
      break;
    }
  }
}

void dropOff() {
  int current_floor = elevatorCar.current_floor->id;
  passengerNode *head = elevatorCar.passengerArray[current_floor];
  passengerNode *next_node;

  while (head != NULL) {
    next_node = head->next;
    kfree(head);
    head = next_node;
  }
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