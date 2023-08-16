PWD := $(shell pwd)
obj-m += test.o
   
all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
install:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules_install
	echo ${INSTALL_MOD_PATH}
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
