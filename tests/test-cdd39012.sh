if [ ! -x  ${ROOTFS}/bin/ptrace ] || [ ! -x  ${ROOTFS}/bin/ptrace-2 ] || [ ! -x  ${ROOTFS}/bin/true ]; then
    exit 125;
fi

${PROOT} -r ${ROOTFS} ptrace
${PROOT} -r ${ROOTFS} ptrace 2

${PROOT} -r ${ROOTFS} ptrace-2 /bin/true

${PROOT} -r ${ROOTFS} ptrace-2 /bin/fork-wait
${PROOT} -r ${ROOTFS} ptrace-2 /bin/fork-wait 2
