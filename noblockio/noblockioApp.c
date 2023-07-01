#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "poll.h"
#include "sys/select.h"
#include "time.h"


/************************************
 * 中断检测按键，定时器消抖实验的测试程序
*************************************/

int main(int argc, char *argv[])
{
        int fd;
        int ret = 0;
        char *filename;
        struct pollfd fds;
        fd_set readfds;
        struct timeval timeout;
        unsigned char data=0;

        if (argc != 2) {
                printf("Error Usage,need one external arg\r\n");
                return -1;
        }

        filename = argv[1];
        fd = open(filename, O_RDWR|O_NONBLOCK);
        if (fd < 0) {
                printf("open %s failed\r\n", filename);
                return -1;
        }

#if 1
        fds.fd = fd;
        fds.events = POLLIN;
        while (1)
        {       
                ret = poll(&fds, 1, 1000);       // 结构体数组 1个设备 最长阻塞1000ms
                if (ret) {                      //从驱动看出 非0则资源可用
                        ret = read(fd, &data, sizeof(data));
                        if (ret < 0)
                                ;
                        else
                                printf("key value = %d \r\n", data);
                } else if (ret == 0) {                  // 超时
                        printf("poll time out\r\n");
                } else {
                        ;
                }
        }
#endif

#if 0
        while (1) {
                FD_ZERO(&readfds);
                FD_SET(fd, &readfds);

                timeout.tv_sec = 1;
                timeout.tv_usec = 0;
                ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
                switch (ret)
                {
                case 0:
                        printf("select time out\r\n");
                        break;  // 超时
                case -1:        // 出错
                        break;
                default:                                        // 返回掩码>0,readfds位图中有设备就绪
                        if (FD_ISSET(fd, &readfds)) {
                                ret = read(fd, &data, sizeof(data));
                                if (ret < 0)
                                        printf("data valid but read failed\r\n");
                                else
                                        printf("key value = %d \r\n", data);
                        }
                        break;
                }
        }
#endif

        close(fd);
        return ret;
}