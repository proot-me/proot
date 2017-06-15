if [ ! -x  ${ROOTFS}/bin/readlink ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} -r ${ROOTFS} /bin/readlink /bin/abs-true | grep "`which true`"
${PROOT} -r ${ROOTFS} /bin/readlink /bin/rel-true | grep '^\./true$'

${PROOT} -b /:/host-rootfs -r ${ROOTFS} /bin/readlink /bin/abs-true | grep "`which true`"
${PROOT} -b /:/host-rootfs -r ${ROOTFS} /bin/readlink /bin/rel-true | grep '^./true$'
