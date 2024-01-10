KERNELDIR := /home/ubuntu/linux/linux-imx-rel_imx_4.1.15_2.1.0_ga_transplant 

PWD := $(shell pwd)

obj-m := icm20608.o

build:kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules CFLAGS=$(CFLAGS)

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean