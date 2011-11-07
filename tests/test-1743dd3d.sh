TMP=/tmp/`mcookie`
rm -f ${ROOTFS}/${TMP}

mkdir -p ${ROOTFS}/tmp
echo '#!/bin/true' > ${ROOTFS}/${TMP}

chmod -x ${ROOTFS}/${TMP}
! ${PROOT} ${ROOTFS} ${TMP}

chmod +x ${ROOTFS}/${TMP}
${PROOT} ${ROOTFS} ${TMP}
