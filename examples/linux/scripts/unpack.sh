#!/bin/bash

mkdir -p rootfs
cp rootfs.cpio rootfs/
cd rootfs
cpio -idv < rootfs.cpio
rm rootfs.cpio
cd ../
