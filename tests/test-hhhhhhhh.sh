if [ ! -x  ${ROOTFS}/bin/true ] || [ -z `which mcookie` ] || [ -z `which true` ] || [ -z `which mkdir` ] || [ -z `which ln` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir -p ${ROOTFS}/${TMP}

A=$(mcookie)
B=$(mcookie)

ln -s /bin/true   ${ROOTFS}/${TMP}/${A}
ln -s ${TMP}/${A} ${ROOTFS}/${TMP}/${B}

env PATH=${TMP} ${PROOT} ${ROOTFS} ${B}

rm -fr ${ROOTFS}/${TMP}
