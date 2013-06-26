if [ ! -x  ${ROOTFS}/bin/ptrace1 ]; then
    exit 125;
fi

${PROOT} -r ${ROOTFS} ptrace1
