if [ ! -x  ${ROOTFS}/bin/fork-wait ] || [ ! -x  ${ROOTFS}/bin/fork-wait ] || [ -z `which strace` ]; then
    exit 125;
fi

${PROOT} strace ${ROOTFS}/bin/fork-wait
${PROOT} strace ${ROOTFS}/bin/fork-wait 2

# TODO: ${PROOT} strace -f ${ROOTFS}/bin/fork-wait
# TODO: ${PROOT} strace -f ${ROOTFS}/bin/fork-wait 2

