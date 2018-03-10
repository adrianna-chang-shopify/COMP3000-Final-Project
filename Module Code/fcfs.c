#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

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
  int origin;
  int destination;
  struct passengerNode* next;
} passengerNode;

typedef struct floorQueue {
  passengerNode* startQueue;
  passengerNode* endQueue;
} floorQueue;

floorQueue shaftArray[NUM_FLOORS];

typedef struct elevatorCar {
  floorQueue* current_floor;
  int passengerCount;
  // array of passengerNode pointers; each array element represents a queue of passengers for a given floor,
  //with the first passenger in the queue being the one with the lowest priority id
  passengerNode* passengerArray[NUM_FLOORS];
} elevatorCar;

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
  // append received message
  sprintf(message, "%s = %d bytes", buffer, (int)len);
  printk(KERN_INFO "first_come_first_served: received %d characters from the user\n", (int)len);
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
