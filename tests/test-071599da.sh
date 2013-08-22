if [ -z `which uname` ] || [ -z `which true` ] || [ -z `which env` ] || [ -z `which grep` ] || [ -z `which tail` ] || [ ! -x  ${ROOTFS}/bin/true ]; then
    exit 125;
fi

${PROOT} -k $(uname -r) true
env PROOT_FORCE_KOMPAT=1 ${PROOT} -k $(uname -r) true

${PROOT} -k $(uname -r) ${ROOTFS}/bin/true
env PROOT_FORCE_KOMPAT=1 ${PROOT} -k $(uname -r) ${ROOTFS}/bin/true

if env LD_SHOW_AUXV=1 true | grep -q ^AT_RANDOM; then
    env PROOT_FORCE_KOMPAT=1 ${PROOT} -k $(uname -r) env LD_SHOW_AUXV=1 true | tail -1 | grep ^AT_RANDOM
fi

! ${PROOT} -k $(uname -r) env LD_SHOW_AUXV=1 true | grep AT_SYSINFO
[ $? -eq 0 ]
