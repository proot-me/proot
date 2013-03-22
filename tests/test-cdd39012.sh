if [ ! -x  ${ROOTFS}/bin/ptrace ] || [ -z `which true` ]; then
    exit 125;
fi

${PROOT} -r ${ROOTFS} ptrace
# TODO: ${PROOT} -r ${ROOTFS} ptrace 2
