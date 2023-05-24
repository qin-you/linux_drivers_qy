/*
 * @Author: You Qin
 * @Date: 2023-03-20 15:13:36
 * @LastEditTime: 2023-03-20 15:23:35
 * @FilePath: /Linux_Drivers/led/test.c
 * @usage:
 * @Description: test led
 */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/types.h"
#include "sys/stat.h"

#define LEDOFF 0
#define LEDON  1

int main(int argc, char *argv[])
{
    int fd, ret;
    char *filename;
    unsigned char databuf[1];

    if(argc != 3) {
        printf("Error Usage!\r\n");
        return -1;
    }

    filename = argv[1];

    fd = open(filename, O_RDWR);
    if(fd < 0) {
        printf("file open failed!\r\n");
        return -1;
    }

    databuf[0] = atoi(argv[2]);

    ret = write(fd, databuf, sizeof(databuf));
    if (ret < 0) {
        printf("kernel write failed!\r\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;


}
