#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"

/******************************************
 * 测试定时器控制led闪烁 ioctl对命令格式有要求
 * usage: ./timerApp /dev/timer
*******************************************/

/*驱动中定义的命令值*/
#define         CLOSE_CMD       (_IO(0xEF, 0x1))
#define         OPEN_CMD        (_IO(0xEF, 0x2))
#define         SETPERIOD_CMD   (_IO(0xEF, 0x3))

int main(int argc, char *argv[])
{
        int fd, ret;
        char *filename;
        unsigned int cmd;
        unsigned int arg;
        unsigned char str[100];

        if (argc != 2) {
                printf("error usage\r\n");
                return -1;
        }

        filename = argv[1];

        fd = open(filename, O_RDWR);
        if (fd < 0) {
                printf("cannot open file %s\r\n", filename);
                return -1;
        }

        while (1) {
                printf("Input CMD:");
                ret = scanf("%d", &cmd);
                if (ret != 1) {         // 输入错误
                        return -1;
                }

                switch (cmd) {
                        case 1:
                                cmd = CLOSE_CMD;
                                break;
                        case 2:
                                cmd = OPEN_CMD;
                                break;
                        case 3:
                                cmd = SETPERIOD_CMD;
                                printf("Input Timer Period:");
                                ret = scanf("%d", &arg);
                                if (ret != 1)
                                        return -1;
                }
                ioctl(fd, cmd, arg);
        }

        close(fd);
}