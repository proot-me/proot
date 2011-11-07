TMP=/tmp/`mcookie`
rm -f ${ROOTFS}/${TMP}
mkdir -p ${ROOTFS}/tmp
cp ${ROOTFS}/bin/true ${ROOTFS}/${TMP}
${PROOT} -e ${ROOTFS} ${TMP}
