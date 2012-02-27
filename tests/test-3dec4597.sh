if [ ! -x  ${ROOTFS}/bin/pwd ]; then
    exit 125;
fi

${PROOT} -m /tmp:/longer-tmp -w /longer-tmp ${ROOTFS} /bin/pwd
