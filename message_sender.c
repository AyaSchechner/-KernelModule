
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "message_slot.h"
#include <linux/fs.h>  

void err_exit(void){
    fprintf(stderr, "%s", strerror(errno));
    exit(1);
}

int main(int argc, char** argv){
    if(argc != 4){
        err_exit();
    }
    int fd = open(argv[1], O_WRONLY);
    if (!fd){
        err_exit();       
    }

    unsigned long channel_id = atoi(argv[2]);
    long io_res = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if(io_res != SUCCESS){
        err_exit();     
    }
    char* msg = argv[3];
    size_t length = strlen(msg);
    ssize_t msg_len = write(fd, msg, length);
    if(msg_len != length){
        err_exit();  
    }
    if(close(fd) < 0){
        err_exit();  
    }
    exit(SUCCESS);
}
