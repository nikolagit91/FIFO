#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

#define BUFF_SIZE 180
MODULE_LICENSE("Dual BSD/GPL");

dev_t my_dev_id;
static struct class * my_class;
static struct device * my_device;
static struct cdev * my_cdev;

DECLARE_WAIT_QUEUE_HEAD(readQ);
DECLARE_WAIT_QUEUE_HEAD(writeQ);
struct semaphore sem;

int fifo[16];
int head = 0;
int tail = 0;
int br_elemenata = 0;
int endRead = 0;
int n = 1;

int fifo_open(struct inode * pinode, struct file * pfile);
int fifo_close(struct inode * pinode, struct file * pfile);
ssize_t fifo_read(struct file * pfile, char __user * buffer, size_t length, loff_t * offset);
ssize_t fifo_write(struct file * pfile,
const char __user * buffer, size_t length, loff_t * offset);

int bintodec(int m) {

  int dec = 0;
  int base = 1;
  int temp = m;

  while (temp) {
    int last_digit = temp % 10;
    temp = temp / 10;
    dec += last_digit * base;
    base = base * 2;
  }

  return dec;
}

struct file_operations my_fops = {
  .owner = THIS_MODULE,
  .open = fifo_open,
  .read = fifo_read,
  .write = fifo_write,
  .release = fifo_close,
};

int fifo_open(struct inode * pinode, struct file * pfile) {

  printk(KERN_INFO "Succesfully opened fifo\n\n");
  return 0;
}

int fifo_close(struct inode * pinode, struct file * pfile) {
  printk(KERN_INFO "Succesfully closed fifo\n\n");
  return 0;
}

ssize_t fifo_read(struct file * pfile, char __user * buffer, size_t length, loff_t * offset) {
  int ret;
  char buff[BUFF_SIZE];
  long int len = 0;
  int i;

  for (i = 0; i < n; i++) {
    if (endRead) {
      endRead = 0;
      tail++;
      //return 0;
    }

    if (down_interruptible(&sem))
      return -ERESTARTSYS;

    while (br_elemenata == 0) {
      head = 0;
      tail = 0;
	up(&sem);      
	if (wait_event_interruptible(readQ, (br_elemenata > 0)))
        return -ERESTARTSYS;
      if (down_interruptible(&sem))
        return -ERESTARTSYS;
    }

    if (br_elemenata > 0) {
      if (tail == 16)
        tail = 0;

      len = scnprintf(buff, BUFF_SIZE, "%d ", fifo[tail]);
      ret = copy_to_user(buffer, buff, len);

      if (ret)
        return -EFAULT;

      br_elemenata = br_elemenata - 1;
      printk(KERN_INFO "Uspjesno procitan broj %d na poziciji %d", fifo[tail], tail);
      printk(KERN_INFO "Broj elemenata je %d\n\n", br_elemenata);

      len = 0;
      endRead = 1;

    } else {
      printk(KERN_WARNING "FIFO je PRAZAN\n");

    }
		up(&sem);
  	wake_up_interruptible(&writeQ);
  }
  
  return len;

}

ssize_t fifo_write(struct file * pfile, const char __user * buffer, size_t length, loff_t * offset) {

  char buff[BUFF_SIZE];
  char value[BUFF_SIZE];
  int ret, broj;
  int i, l, k, j;
  int trenutni_str = 0;
  int binarni;
  int error = 0;
  char * p1 = buff;
  char * p2 = buff;
  int mode = 0;

  ret = copy_from_user(buff, buffer, length);
  if (ret)
    return -EFAULT;
  buff[length - 1] = '\0';
  printk(KERN_INFO "buff je : %s\n", buff);
  printk("\n");
  l = strlen(buff);

  if (buff[0] == 'n' && buff[1] == 'u' && buff[2] == 'm' && buff[3] == '=') {
    sscanf(buff, "num=%d", & n);
    if (n < 1 || n > 16)
      printk(KERN_WARNING "POGRESAN FORMAT\n");
    else
      printk(KERN_INFO "Uspjesno br_elemenatatavljeno citanje %d broja odjednom", n);


  } else {
    for (i = 0; i <= l; i++) {

	if (down_interruptible(&sem))
      	return -ERESTARTSYS;
    while (br_elemenata == 16) {
        up(&sem);
        if (wait_event_interruptible(writeQ, (br_elemenata < 16)))
            return -ERESTARTSYS;
        if (down_interruptible(&sem))
            return -ERESTARTSYS;
    }

      if (br_elemenata < 16) {
      if ( * p2 == ';' || ( * p2 == '\0' && mode == 0)) {
        buff[i] = '\0';
        trenutni_str++;
        memcpy(value, p1, strlen(p1) + 1);
        ret = sscanf(value, "0b%s", value);
        k = strlen(value);
        mode = 1;
        for (j = 0; j < k; j++) {
          if (value[j] > 49 || value[j] < 48)
            error = 1;
        }
        if (ret == 1 && k <= 8 && error == 0) {
          kstrtoint(value, 10, & binarni);
          broj = bintodec(binarni);
         
		    	if (head == 16)
		      	head = 0;

		        fifo[head] = broj;
		        br_elemenata = br_elemenata + 1;
		        printk(KERN_INFO "Uspjesno upisan broj %d na poziciju %d\n", broj, head);
		        printk(KERN_INFO "Broj elemenata je %d\n\n", br_elemenata);
		        head = head + 1;
		        p1 = p2 + 1;
		     /* } else
		        printk(KERN_WARNING "FIFO je PUN\n");*/
		    } else {
		      printk(KERN_WARNING "POGRESAN FORMAT %d-og stringa\n\n", trenutni_str);
		      error = 0;
		      p1 = p2 + 1;
        }
      }
      p2++;
    }
	up(&sem);
    wake_up_interruptible(&readQ);
  }
}
  return length;
}

static int __init fifo_init(void) {
  int ret = 0;
  int i = 0;
  sema_init( & sem, 1);

  //Initialize array
  for (i = 0; i < 10; i++)
    fifo[i] = 0;

  ret = alloc_chrdev_region( & my_dev_id, 0, 1, "fifo");
  if (ret) {
    printk(KERN_ERR "failed to register char device\n");
    return ret;
  }
  printk(KERN_INFO "char device region allocated\n");

  my_class = class_create(THIS_MODULE, "fifo_class");
  if (my_class == NULL) {
    printk(KERN_ERR "failed to create class\n");
    goto fail_0;
  }
  printk(KERN_INFO "class created\n");

  my_device = device_create(my_class, NULL, my_dev_id, NULL, "fifo");
  if (my_device == NULL) {
    printk(KERN_ERR "failed to create device\n");
    goto fail_1;
  }
  printk(KERN_INFO "device created\n");

  my_cdev = cdev_alloc();
  my_cdev -> ops = & my_fops;
  my_cdev -> owner = THIS_MODULE;
  ret = cdev_add(my_cdev, my_dev_id, 1);
  if (ret) {
    printk(KERN_ERR "failed to add cdev\n");
    goto fail_2;
  }
  printk(KERN_INFO "cdev added\n");
  printk(KERN_INFO "Hello world\n");

  return 0;

  fail_2:
    device_destroy(my_class, my_dev_id);
  fail_1:
    class_destroy(my_class);
  fail_0:
    unregister_chrdev_region(my_dev_id, 1);
  return -1;
}

static void __exit fifo_exit(void) {
  cdev_del(my_cdev);
  device_destroy(my_class, my_dev_id);
  class_destroy(my_class);
  unregister_chrdev_region(my_dev_id, 1);
  printk(KERN_INFO "Goodbye, cruel world\n");
}

module_init(fifo_init);
module_exit(fifo_exit);