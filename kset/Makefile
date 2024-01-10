KERN_VER = $(shell uname -r)
KERN_DIR = /lib/modules/${KERN_VER}/build
PWD	= $(shell pwd)

obj-m += kset-example.o

all:
	make -C ${KERN_DIR} M=${PWD} modules

.PHONY:clean

clean:
	make -C ${KERN_DIR} M=${PWD} clean

	
