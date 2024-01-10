#!/bin/bash
if ! make;then
        echo "make wrong"
        exit 1
fi
if ! arm-linux-gnueabihf-gcc noblockioApp.c -o noblockioApp;then
        echo "cross-compile chrdevbaseApp wrong"
        exit 2
fi

sudo cp *.ko /home/ubuntu/linux/nfs/rootfs/lib/modules/4.1.15/noblockio/
sudo cp *App /home/ubuntu/linux/nfs/rootfs/lib/modules/4.1.15/noblockio/
