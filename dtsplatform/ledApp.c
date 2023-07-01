#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

/*****************************
 * platform设备驱动 测试led 
 * usage:  ./ledApp /dev/xxled 0
******************************/

#define LEDOFF 0
#define LEDON  1

int main(int argc, char *argv[])
{
        int fd, ret;
        char *filename;
        unsigned char databuf[1];

        if (argc != 3) {
                printf("arg num should be 3\r\n");
                return -1;
        }

        filename = argv[1];
        fd = open(filename, O_RDWR);
        if (fd < 0) {
                printf("file %s open failed\r\n", filename);
                return -1;
        }

        databuf[0] = atoi(argv[2]);
        ret = write(fd, databuf, sizeof(databuf));
        if (ret < 0) {
                printf("LED control failed \r\n");
                close(fd);
                return -1;
        }

        close(fd);
        return 0;
}