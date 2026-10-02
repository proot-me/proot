if [ ! -x  ${ROOTFS}/bin/readlink ] || [ -z `which realpath` ] || [ -z `which grep` ]; then
    exit 125;
fi

RESULT=$(realpath ${ROOTFS})

${PROOT} -b /proc -r ${ROOTFS} readlink /proc/self/root | grep ^${RESULT}$
