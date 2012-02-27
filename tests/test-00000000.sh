if [ ! -x ${ROOTFS}/bin/true ]; then
    exit 125;
fi

${PROOT} ${ROOTFS} /bin/true
