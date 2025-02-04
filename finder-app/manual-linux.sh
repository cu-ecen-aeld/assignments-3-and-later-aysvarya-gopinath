#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
#Modified by:Aysvarya Gopinath
#REFERENCES:https://askubuntu.com/questions/1376461/copy-all-from-a-folder-into-the-project-root-directory

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    #  kernel building 
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper #deep clean the kernel build tree
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig #configure our virtual dev board
make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all # build kernel image for booting
#make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules #build modules
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs #build device tree

fi

echo "Adding the Image in outdir"
cp -r ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image  ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# Creating necessary base directories
echo " root file system"
mkdir ${OUTDIR}/rootfs  #root files system
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # Configuring  busybox
    make distclean
    make defconfig

else
    cd busybox
fi

echo "installing busy box"
#installing busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

echo "adding library dependencies to rootfs"

export SYSROOT=$(aarch64-none-linux-gnu-gcc -print-sysroot)
sudo cp -L $SYSROOT/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
sudo cp -L $SYSROOT/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
sudo cp -L $SYSROOT/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
sudo cp -L $SYSROOT/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64


echo "creating device nodes"
cd "${OUTDIR}/rootfs"
#device nodes
sudo rm -f /dev/null #cleanup
sudo rm -f /dev/console

sudo mknod -m 666 "/dev/null" c 1 3 # null device
sudo mknod -m 600 "/dev/console" c 5 1 # console device 

# Clean and build the writer utility
echo "making the writer app"
cd ${FINDER_APP_DIR} 
make clean
make CROSS_COMPILE=${CROSS_COMPILE} #make with arm compiler

# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "copying finder scripts to new root file system"
cp -a ./writer ${OUTDIR}/rootfs/home  # -a preserves attributes
cp -a ./finder.sh ${OUTDIR}/rootfs/home 
cp -r ./conf/ ${OUTDIR}/rootfs/home #/. copies hidden files 
cp -a ./finder-test.sh ${OUTDIR}/rootfs/home
cp -a ./autorun-qemu.sh  ${OUTDIR}/rootfs/home
cp -a ./writer.c ${OUTDIR}/rootfs/home
cp -a ./Makefile  ${OUTDIR}/rootfs/home


#Chown the root directory
echo "giviing root permissions to file system"
cd "${OUTDIR}/rootfs"
sudo chown -R root:root * #make contents owned by root

# Create initramfs.cpio.gz
#cd "$OUTDIR/rootfs"
echo "creating initramfs image"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
#cd ${OUTDIR}
gzip -f initramfs.cpio




