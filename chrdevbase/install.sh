#!/bin/bash
if ! make;then
        echo "make wrong"
        exit 1
fi
if ! arm-linux-gnueabihf-gcc chrdevbaseApp.c -o chrdevbaseApp;then
        echo "cross-compile chrdevbaseApp wrong"
        exit 2
fi

#cp chrdevbase.ko /home/ubuntu/linux/nfs/rootfs/lib/modules/4.1.15/
#cp chrdevbaseApp /home/ubuntu/linux/nfs/rootfs/lib/modules/4.1.15/



# board:
#./chrdevbaseApp /dev/chrdevbase 1
#./chrdevbaseApp /dev/chedevbase 2
