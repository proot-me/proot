if [ -z `which true` ] || [ -z `which realpath` ] || [ -z `which grep` ] || [ -z `which env` ] || [ ! -x  ${ROOTFS}/bin/puts_proc_self_exe ]; then
    exit 125;
fi

TRUE=$(realpath $(which true))

env PROOT_FORCE_FOREIGN_BINARY=1 ${PROOT} -q ${ROOTFS}/bin/puts_proc_self_exe ${TRUE} | grep ^${TRUE}$
