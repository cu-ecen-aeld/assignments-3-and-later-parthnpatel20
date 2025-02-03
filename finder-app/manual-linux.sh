#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.178
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


cd linux-stable
echo "Checking out version ${KERNEL_VERSION}"
git checkout ${KERNEL_VERSION}

echo "Building Kernel..."
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/
echo "Kernel Image copied to ${OUTDIR}"


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]; then
	echo "Deleting existing rootfs directory"
    sudo rm -rf ${OUTDIR}/rootfs
fi

mkdir -p ${OUTDIR}/rootfs
mkdir -p ${OUTDIR}/rootfs/bin
mkdir -p ${OUTDIR}/rootfs/sbin
mkdir -p ${OUTDIR}/rootfs/lib
mkdir -p ${OUTDIR}/rootfs/lib64
mkdir -p ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/dev
mkdir -p ${OUTDIR}/rootfs/proc
mkdir -p ${OUTDIR}/rootfs/sys
mkdir -p ${OUTDIR}/rootfs/tmp

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]; then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig
else
    cd busybox
fi

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install CONFIG_PREFIX=${OUTDIR}/rootfs


SYSROOT=$(${CROSS_COMPILE}gcc --print-sysroot)

cp -a ${SYSROOT}/lib/ld-2.*.so ${OUTDIR}/rootfs/lib/
cp -a ${SYSROOT}/lib/libm.so.6 ${OUTDIR}/rootfs/lib/
cp -a ${SYSROOT}/lib/libc.so.6 ${OUTDIR}/rootfs/lib/
cp -a ${SYSROOT}/lib64 ${OUTDIR}/rootfs/


sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1


cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
cp writer ${OUTDIR}/rootfs/home/


cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/

sed -i 's|\.\./conf/assignment.txt|conf/assignment.txt|' ${OUTDIR}/rootfs/home/finder-test.sh

sudo chown -R root:root ${OUTDIR}/rootfs


cd ${OUTDIR}/rootfs
find . | cpio -o -H newc | gzip > ${OUTDIR}/initramfs.cpio.gz



