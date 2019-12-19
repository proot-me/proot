if [ ! -x  ${ROOTFS}/bin/readlink ] || [ -z `which mcookie` ] || [ -z `which touch` ] || [ -z `which mkdir` ] || [ -z `which ln` ] || [ -z `which grep` ] || [ -z `which rm` ]; then
    exit 125;
fi

REGULAR=`mcookie`
SYMLINK=`mcookie`

touch /tmp/${REGULAR}
ln -fs /tmp/${REGULAR} /tmp/${SYMLINK}

mkdir -p ${ROOTFS}/tmp
touch ${ROOTFS}/tmp/${REGULAR}
ln -fs /tmp/${REGULAR} ${ROOTFS}/tmp/${SYMLINK}

${PROOT} -b /tmp:/ced -r ${ROOTFS} /bin/readlink /tmp/${SYMLINK} | grep ^/tmp/${REGULAR}$
${PROOT} -b /tmp:/ced -r ${ROOTFS} /bin/readlink /ced/${SYMLINK} | grep ^/ced/${REGULAR}$

rm -f /tmp/${REGULAR}
rm -f /tmp/${SYMLINK}
rm -f ${ROOTFS}/tmp/${REGULAR}

