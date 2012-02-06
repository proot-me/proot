
REGULAR=`mcookie`
SYMLINK=`mcookie`

touch /tmp/${REGULAR}
ln -s /tmp/${REGULAR} /tmp/${SYMLINK}

mkdir -p ${ROOTFS}/tmp
touch ${ROOTFS}/tmp/${REGULAR}
ln -s /tmp/${REGULAR} ${ROOTFS}/tmp/${SYMLINK}

${PROOT} -b /tmp:/ced ${ROOTFS} /bin/readlink /tmp/${SYMLINK} | grep ^/tmp/${REGULAR}$
${PROOT} -b /tmp:/ced ${ROOTFS} /bin/readlink /ced/${SYMLINK} | grep ^/ced/${REGULAR}$

#rm -f /tmp/${REGULAR}
#rm -f /tmp/${SYMLINK}
#rm -f ${ROOTFS}/tmp/${REGULAR}

