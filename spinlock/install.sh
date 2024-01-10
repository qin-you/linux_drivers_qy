#!/bin/bash
if ! make;then
        echo "make wrong"
        exit 1
fi
if ! arm-linux-gnueabihf-gcc spinlockApp.c -o spinlockApp;then
        echo "cross-compile chrdevbaseApp wrong"
        exit 2
fi

#sudo cp chrdevbase.ko /home/ubuntu/linux/nfs/rootfs/lib/modules/4.1.15/
#sudo cp chrdevbaseApp /home/ubuntu/linux/nfs/rootfs/lib/modules/4.1.15/
