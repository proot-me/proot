if [ ! -x  ${ROOTFS}/bin/pwd ] || [ -z `which mkdir` ] || [ -z `which grep` ]; then
    exit 125;
fi

mkdir -p ${ROOTFS}/${PWD}
${PROOT} -v 1 -w . ${ROOTFS} pwd | grep ^${PWD}$
