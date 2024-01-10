#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

#define LEDOFF  0
#define LEDON   1

int main(int argc, char* argv[])
{
        int fd, retvalue, run_time=10, i=1;
        char *filename;
        unsigned char cnt = 0;
        unsigned char databuf[1];

        if (argc != 3) {
                printf("Error args number\r\n");
                return -1;
        }

        filename = argv[1];

        fd = open(filename, O_RDWR);
        if (fd < 0) {
                printf("open device %s failed\r\n", filename);
                return -1;
        }

        // ascii to int: atoi
        databuf[0] = atoi(argv[2]);
        
        retvalue = write(fd, databuf, sizeof(databuf));
        if (retvalue < 0) {
                printf("write failed\r\n");
                close(fd);
                return -1;
        }

        // 占用一段时间
        for (; i <= run_time; i++) {
                sleep(1);
                printf("pid:%d App is using led %d/%d\r\n", getpid(), i, run_time);
        }        

        retvalue = close(fd);
        if (retvalue < 0) {
                printf("close device failed\r\n");
                return -1;
        }
        return 0;
}