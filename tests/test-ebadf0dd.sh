if [ -z `which mcookie` ] || [ -z `which rm` ] || [ ! -x  ${ROOTFS}/bin/true ]; then
    exit 125;
fi

TMP=/tmp/`mcookie`
rm -fr ${ROOTFS}/${TMP}

${PROOT} -b /etc/fstab:/${TMP}/fstab -b /tmp:/${TMP}/tmp ${ROOTFS} /bin/true
test -f ${ROOTFS}/${TMP}/fstab
test -d ${ROOTFS}/${TMP}/tmp

rm -fr ${ROOTFS}/${TMP}
