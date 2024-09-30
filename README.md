Implementation of a kernel module that provides a new IPC mechanism, called a message slot. 
A message slot is a character device file through which processes communicate. 
A message slot device has multiple message channels active concurrently, which can be used by multiple processes.
