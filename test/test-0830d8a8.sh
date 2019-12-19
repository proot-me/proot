if [ ! -x  ${ROOTFS}/bin/exec-m32 ] || [ ! -x  ${ROOTFS}/bin/true ]; then
    exit 125;
fi

${PROOT} ${ROOTFS}/bin/exec-m32 ${ROOTFS}/bin/true
