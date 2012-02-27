if [ ! -x  ${ROOTFS}/bin/readlink ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} ${ROOTFS} /bin/readlink /bin/abs-true | grep '^/bin/true$'
${PROOT} ${ROOTFS} /bin/readlink /bin/rel-true | grep '^\./true$'

${PROOT} -b /:/host-rootfs ${ROOTFS} /bin/readlink /bin/abs-true | grep '^/bin/true$'
${PROOT} -b /:/host-rootfs ${ROOTFS} /bin/readlink /bin/rel-true | grep '^./true$'
