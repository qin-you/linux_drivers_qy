#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
/***************************************************************
13 描述 : chrdevbase 驱测试 APP。
14 其他 : 使用方法： ./chrdevbaseApp /dev/chrdevbase <1>|<2>
15 argv[2] 1:读文件
16 argv[2] 2:写文件
***************************************************************/

static char usrdata[] = {"user data!"};

/*
* @description : main 主程序
* @param - argc : argv 数组元素个数
* @param - argv : 具体参数
* @return : 0 成功;其他 失败
*/
int main(int argc, char *argv[])
{
        int fd, retvalue;
        char *filename;
        char readbuf[100], writebuf[100];

        if(argc != 3){
                printf("Error Usage!\r\n");
                return -1;
        }

        filename = argv[1];

        /* 打开驱动文件 */
        fd = open(filename, O_RDWR);
        if(fd < 0){
                printf("Can't open file %s\r\n", filename);
                return -1;
        }

        if(atoi(argv[2]) == 1){ /* 从驱动文件读取数据 */
                retvalue = read(fd, readbuf, 50);
                if(retvalue < 0) {
                        printf("read file %s failed!\r\n", filename);
                } else{
                /* 读取成功，打印出读取成功的数据 */
                        printf("read data:%s\r\n",readbuf);
                }
        }

        if(atoi(argv[2]) == 2){
                /* 向设备驱动写数据 */
                memcpy(writebuf, usrdata, sizeof(usrdata));
                retvalue = write(fd, writebuf, 50);
                if(retvalue < 0){
                        printf("write file %s failed!\r\n", filename);
                }
                printf("write success\r\n");
        }

        /* 关闭设备 */
        retvalue = close(fd);
        if(retvalue < 0){
                printf("Can't close file %s\r\n", filename);
                return -1;
        }

        return 0;
}
