if [ ! -e ${ROOTFS}/bin/true-pie ] || [ ! -e ${ROOTFS}/bin/false-pie ]; then
    exit 125
fi

${PROOT} ${ROOTFS}/bin/true-pie

! ${PROOT} ${ROOTFS}/bin/false-pie
[ $? -eq 0 ]
