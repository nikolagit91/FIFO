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
#define BUFF_SIZE 20
MODULE_LICENSE("Dual BSD/GPL");

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;

int fifo[10];
int pos = 0;
int endRead = 0;
int temp=0;

int fifo_open(struct inode *pinode, struct file *pfile);
int fifo_close(struct inode *pinode, struct file *pfile);
ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);



int bintodec(int m)
{
   
	int dec = 0;
	int base = 1;
	int temp = m;

	while (temp)
	{
		int last_digit = temp % 10;
		temp = temp / 10;
		dec += last_digit * base;
		base = base * 2;
	}
	
	return dec;
}


struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = fifo_open,
	.read = fifo_read,
	.write = fifo_write,
	.release = fifo_close,
};


int fifo_open(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully opened fifo\n");
		return 0;
}

int fifo_close(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully closed fifo\n");
		return 0;
}

ssize_t fifo_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) 
{
	int ret;
	char buff[BUFF_SIZE];
	long int len = 0;
	if (endRead){
		endRead = 0;
		temp++;
		return 0;
	}

	if(pos > 0)
	{
				
		pos--;		
		len = scnprintf(buff, BUFF_SIZE, "%d ", fifo[temp]); //vraca broj karatkera upisanih u buff
		ret = copy_to_user(buffer, buff, len); // vraca broj bajtova koji nije upisan
		if(ret)
			return -EFAULT;
		printk(KERN_INFO "Succesfully read\n");
		
		endRead = 1;
	}
	else
	{
		printk(KERN_WARNING "fifo is empty\n");
		temp=0; 
	}

	return len;
}


ssize_t fifo_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) 
{
	
	char buff[BUFF_SIZE];// binarni[16][8];
	char value[BUFF_SIZE];
	int ret;
	int i;
	int l;
	int binarni;
	int broj;
	int error=0;


	temp=0;
	ret = copy_from_user(buff, buffer, length); 	// vraca broj bajtova koji nije upisan
	if(ret)
		return -EFAULT;
	buff[length-1] = '\0';

	
	if(pos<16)
	{
		ret = sscanf(buff,"0b%s;",value);
		l=strlen(value);
			
		for (i=0; i < l; i++) {
			if  (value[i]>49 || value[i]<48) 
				error=1;
						
			} 

		if(ret==1 && l<= 8 && error==0)				
		{

			kstrtoint(value,10,&binarni); 
			broj=bintodec(binarni);		
			printk(KERN_INFO "Succesfully wrote value %d on position %d", broj, pos); 
			fifo[pos] = broj; 
			pos=pos+1;
		}	
				
			else {
			printk(KERN_WARNING "Wrong command format\n");			
			error=0;			
			}
	}
	else 
		printk(KERN_WARNING "fifo is full\n"); 
	
return length;
}


static int __init fifo_init(void)
{
   int ret = 0;
	int i=0;

	//Initialize array
	for (i=0; i<10; i++)
		fifo[i] = 0;

   ret = alloc_chrdev_region(&my_dev_id, 0, 1, "fifo");
   if (ret){
      printk(KERN_ERR "failed to register char device\n");
      return ret;
   }
   printk(KERN_INFO "char device region allocated\n");

   my_class = class_create(THIS_MODULE, "fifo_class");
   if (my_class == NULL){
      printk(KERN_ERR "failed to create class\n");
      goto fail_0;
   }
   printk(KERN_INFO "class created\n");
   
   my_device = device_create(my_class, NULL, my_dev_id, NULL, "fifo");
   if (my_device == NULL){
      printk(KERN_ERR "failed to create device\n");
      goto fail_1;
   }
   printk(KERN_INFO "device created\n");

	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
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

static void __exit fifo_exit(void)
{
   cdev_del(my_cdev);
   device_destroy(my_class, my_dev_id);
   class_destroy(my_class);
   unregister_chrdev_region(my_dev_id,1);
   printk(KERN_INFO "Goodbye, cruel world\n");
}


module_init(fifo_init);
module_exit(fifo_exit);
