if [ ! -x  ${ROOTFS}/bin/fork-wait-1 ] || [ ! -x  ${ROOTFS}/bin/fork-wait-2 ] || [ -z `which strace` ]; then
    exit 125;
fi

${PROOT} strace ${ROOTFS}/bin/fork-wait-1
${PROOT} strace ${ROOTFS}/bin/fork-wait-2

# TODO: ${PROOT} strace -f ${ROOTFS}/bin/fork-wait-1
# TODO: ${PROOT} strace -f ${ROOTFS}/bin/fork-wait-2

