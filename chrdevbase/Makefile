PWD := $(shell pwd)
obj-m += chrdevbase.o

MY-CFLAGS := -I/lib/modules/`uname -r`/build/include/linux
   
all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) EXTRA_CFLAGS=${MY-CFLAGS} modules
install:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules_install
	echo ${INSTALL_MOD_PATH}
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
