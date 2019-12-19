#!/bin/sh

if [ ! -x ${ROOTFS}/bin/true -o ! -x ${ROOTFS}/bin/chroot ]; then
    exit 125;
fi

${PROOT} -0 -r ${ROOTFS} -w / /bin/chroot . /bin/true
