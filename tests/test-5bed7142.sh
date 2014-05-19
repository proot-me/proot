if [ ! -x  ${ROOTFS}/bin/pwd ] || [ -z `which mkdir` ] || [ -z `which grep` ] || [ -z `which mcookie` ]; then
    exit 125;
fi

mkdir -p ${ROOTFS}/${PWD}
${PROOT} -v 1 -w . -r ${ROOTFS} pwd | grep ^${PWD}$

TMP=/tmp/$(mcookie)
mkdir ${TMP}
! ${PROOT} sh -c "cd ${TMP}; rmdir ${TMP}; pwd -P"
[ $? -eq 0 ]
