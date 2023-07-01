#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/input.h>

/**************************************
 * 测试input 按键驱动
 * @usage: ./inputApp
**************************************/

static struct input_event inputevent;

int main(int argc, char *argv[])
{
	int 		fd;
	int		err = 0;
	char		*filename;

	filename = argv[1];

	if (argc != 2) {
		printf("args num error\r\n");
		return -1;
	}

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("open %s failed\r\n", filename);
		return -1;
	}

	while (1) {
		err = read(fd, &inputevent, sizeof(inputevent));
		if (err > 0) {
			switch (inputevent.type) {
			case EV_KEY:
				if (inputevent.code < BTN_MISC)	// 键盘键值
					printf("key %d %s\r\n", inputevent.code, inputevent.value ? "pressed" : "released");
				else 
					printf("button %d %s\r\n", inputevent.code, inputevent.value?"pressed":"released");
				break;
			case EV_REL:
				break;
			case EV_ABS:
				break;
			case EV_MSC:
				break;
			case EV_SW:
				break;
			default:
				break;
			}
		} else {
			printf("read failed\r\n");
			return -1;
		}
	}

	return 0;
}