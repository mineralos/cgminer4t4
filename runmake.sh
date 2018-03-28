#!/bin/sh

#make clean
#make distclean

./autogen.sh

ROOTFS_DIR=/home/Users/wangp/lib/rootfs
MAKE_JOBS=4
CHIP_TYPE=A8


LDFLAGS="-L${ROOTFS_DIR}/lib -lim_lib -lim_drv " \
CFLAGS="-I${ROOTFS_DIR}/include " \
./configure --prefix=${ROOTFS_DIR} \
--enable-bitmine_${CHIP_TYPE} --without-curses --host=arm-xilinx-linux-gnueabi --build=x86_64-pc-linux-gnu # --target=arm

make -j${MAKE_JOBS}

#cp ./cgminer /home/public/update/cgminer_ccx.$1
#chmod 777 /home/public/update/cgminer_ccx.$1

