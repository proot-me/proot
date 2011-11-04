TMP=`mcookie`
test ! -e ${TMP}
rm -fr ${ROOTFS}/${TMP}

${PROOT} -b /etc/fstab:/${TMP}/fstab -b /tmp:/${TMP}/tmp ${ROOTFS} /bin/true
test -f ${ROOTFS}/${TMP}/fstab
test -d ${ROOTFS}/${TMP}/tmp

rm -fr ${ROOTFS}/${TMP}
