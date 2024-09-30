// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <stddef.h>
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include "message_slot.h"
#include <linux/slab.h>


//// channel list limit to 256???

MODULE_LICENSE("GPL");

typedef struct Device Device;
typedef struct Message Message;
typedef struct Channel Channel;

typedef struct Message{
  ssize_t message_len;
  char message[BUF_LEN];
}Message;

typedef struct Channel{
  int channel_id;
  Channel* next;
  Message* message;
}Channel;

typedef struct Device{
  int minor_id;
  Device* next; // next minor
  Channel* channel;
}Device;

struct chardev_info {
  spinlock_t lock;
};

Device* minor_head = NULL;

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode* inode, struct file* file){
  Device* device;
  Device* prev_d;
  int minor_id = iminor(inode);
  int is_null = 0;

  // check if minor_id exists 
  if(minor_head != NULL){
    is_null = 1;
    device = minor_head;
    while (device != NULL){
      if(device -> minor_id == minor_id){
        return SUCCESS;
      }
      prev_d = device;
      device = device -> next;
    }
  }
  // open a device 
  device = (Device*)kmalloc(sizeof(Device), GFP_KERNEL);
  if(!device){
    printk("Failed to allocate memory");
      return -EINVAL;
  }

  device -> minor_id = minor_id;
  device -> channel = NULL;
  device -> next = NULL;
  if(is_null){
    prev_d -> next = device;
  }
  else{
    minor_head = device;
  }
  return SUCCESS;
}

//---------------------------------------------------------------
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param){
  Device* device;
  Channel* new_channel;
  Channel* prev_ch;
  Channel* cur_ch;
  int minor_id;
  int channel_exist = 0;
  if (ioctl_param == 0 || ioctl_command_id != MSG_SLOT_CHANNEL || minor_head == NULL){
    return -EINVAL;
  }
    
  minor_id = iminor(file -> f_inode);
  device = minor_head;
  while (device != NULL){
    if(device -> minor_id == minor_id){
      break;
    }
    device = device -> next;
  }
  if(device -> channel != NULL){
    channel_exist = 1;
    cur_ch = device -> channel;
    // check if channel_id is in channel
    while (cur_ch != NULL){
      if(cur_ch -> channel_id == ioctl_param){
          file -> private_data = (void*)cur_ch;
          return SUCCESS;
      }
      prev_ch = cur_ch;
      cur_ch = cur_ch -> next;
    }
  }

  // add channel_id
  new_channel = (Channel*)kmalloc(sizeof(Channel), GFP_KERNEL);
  if(!new_channel){
    printk("Failed to allocate memory");
    return -EINVAL;
  }
  new_channel -> channel_id = ioctl_param;
  new_channel -> message = NULL;
  new_channel -> next = NULL;
  file -> private_data = (void*)new_channel;
  
  // channel_id not in channel list
  if(channel_exist){
  prev_ch -> next = new_channel;
  }
  // device channel is null
  else{
  device -> channel = new_channel;
  }
  return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset){
  void* data = file -> private_data;
  Channel* file_channel;
  Message* msg;
  int i;
  if(data == NULL || buffer == NULL){
      return -EINVAL;
  }
  file_channel = (Channel*)data;
  msg = file_channel -> message;
  if(msg == NULL){
    return -EWOULDBLOCK;
    }
  if(msg -> message_len > length){
    return -ENOSPC;  
  }

  for(i = 0; i < msg -> message_len ; i++){
    int read_i = put_user(msg -> message[i], &buffer[i]);
    if (read_i != 0){
      return -EINVAL; 
    }
  }
  return i;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset){
  void* data = file -> private_data;
  ssize_t msg_len;
  Channel* file_channel;
  Message* msg;
  if(data == NULL || buffer == NULL){
      return -EINVAL;
  }
  if(length == 0 || length > BUF_LEN){
    return -EMSGSIZE;
  }

  file_channel = (Channel*)data;
  msg = kmalloc(sizeof(Message), GFP_KERNEL); // not sure if its good
  if(file_channel == NULL || msg == NULL){
    return -EINVAL;  
  }
  
  for(msg_len = 0; msg_len < length ; msg_len++){
    int write_i = get_user(msg -> message[msg_len], &buffer[msg_len]);
    if (write_i != 0){
      kfree(msg);
      return -EINVAL; 
    }
  }
  // updete channel and message
  msg -> message_len = msg_len;
  file_channel -> message = msg;
  return msg_len;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
  .owner	        = THIS_MODULE, // Required for correct count of module usage. This prevents the module from being removed while used.
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void){  
  int ret = register_chrdev(MAJOR_SLOT, DEVICE_RANGE_NAME,&Fops);

  if(ret < 0){
    printk(KERN_ALERT "%s registraion failed for %d\n", DEVICE_FILE_NAME, MAJOR_SLOT);
    return ret;
  }
  printk("init was successful.");
  return 0;
}

//---------------------------------------------------------------
static void free_channel(void){
  Device* minor = minor_head;
  Channel* cur_channel;
  Channel* next_channel;
  while (minor != NULL){
    cur_channel = minor -> channel;
    while (cur_channel != NULL){
      next_channel = cur_channel -> next;
      kfree(cur_channel -> message);
      cur_channel = next_channel;
    }
  }
}

static void __exit simple_cleanup(void){
  // Unregister the device
  // Should always succeed
  unregister_chrdev(MAJOR_SLOT, DEVICE_RANGE_NAME);
  free_channel();
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
