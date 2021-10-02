if [ ! -x  ${ROOTFS}/bin/gdb-ptrace-test ]; then
    exit 125;
fi

${ROOTFS}/bin/gdb-ptrace-test
${ROOTFS}/bin/gdb-ptrace-test 1
${PROOT} ${ROOTFS}/bin/gdb-ptrace-test
${PROOT} ${ROOTFS}/bin/gdb-ptrace-test 1
