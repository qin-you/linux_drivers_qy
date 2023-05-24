#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

/*****************************
 * 按键测试程序:循环打印按键状态
 * usage:./keyApp /dev/key
******************************/

#define KEY0VALUE       0xF0
#define INVAKEY         0x00

int main(int argc, char *argv[])
{
        int fd, ret;
        char *filename;
        unsigned char keyvalue;

        if (argc != 2) {
                printf("error usage\r\n");
                return -1;
        }

        filename = argv[1];
        
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
                printf("open device failed\r\n");
                return -1;
        }

        while(1) {
                read(fd, &keyvalue, sizeof(keyvalue));
                if (keyvalue == KEY0VALUE)
                        printf("KEY0 pressed, value=%#X\r\n", keyvalue);
        }

        ret = close(fd);
        if (ret < 0) {
                printf("close device failed\r\n");
                return -1;
        }
        
        return 0;
}