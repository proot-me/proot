if [ ! -x  ${ROOTFS}/bin/true ] || [ -h /bin/true ] || [ -h /bin ] || [ -z `which mcookie` ] || [ -z `which true` ] || [ -z `which mkdir` ] || [ -z `which ln` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir -p ${ROOTFS}/${TMP}

A=$(mcookie)
B=$(mcookie)

! ln -s /bin/true   -r ${ROOTFS}/${TMP}/${A}
! ln -s ${TMP}/${A} -r ${ROOTFS}/${TMP}/${B}

if [ ! -e ${ROOTFS}/${TMP}/${A} ]; then
    exit 125;
fi

env PATH=${TMP} ${PROOT} -r ${ROOTFS} ${B}

rm -f ${TMP}/${B}  # just in case it also exists in the host env.
${PROOT} -r ${ROOTFS} /${TMP}/${B}

rm -fr ${ROOTFS}/${TMP}
