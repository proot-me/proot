if [ ! -x ${ROOTFS}/bin/true ] || [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which echo` ] || [ -z `which chmod` ]; then
    exit 125;
fi

TMP=/tmp/`mcookie`
rm -f ${ROOTFS}/${TMP}

mkdir -p ${ROOTFS}/tmp
echo '#!/bin/true' > ${ROOTFS}/${TMP}

chmod -x ${ROOTFS}/${TMP}
! ${PROOT} -r ${ROOTFS} ${TMP}

chmod +x ${ROOTFS}/${TMP}
${PROOT} -r ${ROOTFS} ${TMP}

rm -f ${ROOTFS}/${TMP}
