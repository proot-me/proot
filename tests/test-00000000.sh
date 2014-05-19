if [ ! -x ${ROOTFS}/bin/true ]; then
    exit 125;
fi

${PROOT} -r ${ROOTFS} /bin/true
