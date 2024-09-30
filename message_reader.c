
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
    if(argc != 3){
        err_exit();
    }
    int fd = open(argv[1], O_RDONLY);
    if (!fd){
        err_exit();       
    }

    unsigned long channel_id = atoi(argv[2]);
    long io_res = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if(io_res != SUCCESS){
        err_exit();     
    }
    char buffer[BUF_LEN];
    ssize_t msg_len = read(fd, buffer, BUF_LEN);
    if(msg_len < 0){
        err_exit();  
    }
    if(close(fd) < 0){
        err_exit();  
    }
    int msg_write = write(STDOUT_FILENO, buffer, msg_len);
    if(msg_write < 0){
        err_exit();  
    }

    exit(SUCCESS);
}