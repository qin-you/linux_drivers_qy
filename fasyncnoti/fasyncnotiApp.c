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
#include "linux/ioctl.h"
#include "signal.h"

/********************************************************
 * 中断检测按键，异步通知，收到SIGIO信号就用其处理把按键值打印
*********************************************************/

static int fd = 0;

static void sigio_func(int signum)
{
        int err = 0;
        unsigned int keyvalue = 0;

        err = read(fd, &keyvalue, sizeof(keyvalue));
        if (err < 0)
                printf("read error\r\n");
        else
                printf("got sigio signal! key value = %d\r\n", keyvalue);
}

int main(int argc, char *argv[])
{
        char *filename;
        int flags = 0;


        if (argc != 2) {
                printf("Error Usage,need one external arg\r\n");
                return -1;
        }

        filename = argv[1];
        fd = open(filename, O_RDWR);
        if (fd < 0) {
                printf("open %s failed\r\n", filename);
                return -1;
        }

        /* 指定信号SIGIO的处理函数*/
        signal(SIGIO, sigio_func);

        /* 把自己（本进程）加到驱动的通知链表中 */
        fcntl(fd, F_SETOWN, getpid());
        flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, flags|FASYNC);

        while (1)
                // sleep(2);
                ;

        close(fd);
        return 0;
}