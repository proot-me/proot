if [ ! -x  ${ROOTFS}/bin/readlink ] || [ ! -x  ${ROOTFS}/bin/symlink ] || [ -z `which mcookie` ] || [ -z `which rm` ] || [ -z `which ln` ] || [ -z `which mkdir` ]; then
    exit 125;
fi

LINK_NAME1=`mcookie`
LINK_NAME2=`mcookie`

rm -f /tmp/${LINK_NAME1}
rm -f /tmp/${LINK_NAME2}

mkdir -p ${ROOTFS}/tmp

ln -s /tmp/ced-host /tmp/${LINK_NAME1}
ln -s /tmp/ced-guest ${ROOTFS}/tmp/${LINK_NAME1}
${PROOT} -r ${ROOTFS} readlink /tmp/${LINK_NAME1} | grep ^/tmp/ced-guest$
${PROOT} -b /tmp -r ${ROOTFS} readlink /tmp/${LINK_NAME1} | grep ^/tmp/ced-host$
${PROOT} -b /tmp:/foo -r ${ROOTFS} readlink /foo/${LINK_NAME1} | grep ^/foo/ced-host$
${PROOT} -b /tmp:/foo -r ${ROOTFS} readlink /tmp/${LINK_NAME1} | grep ^/tmp/ced-guest$

${PROOT} -b /:/host-rootfs -r ${ROOTFS} readlink /tmp/${LINK_NAME1} | grep ^/tmp/ced-guest$
${PROOT} -b /:/host-rootfs -b /tmp:/foo -r ${ROOTFS} readlink /tmp/${LINK_NAME1} | grep ^/tmp/ced-guest$

# Always use the deepest binding, deepest from the host point-of-view.
${PROOT} -b /:/host-rootfs -r ${ROOTFS} readlink /tmp/${LINK_NAME1} | grep ^/tmp/ced-guest$
${PROOT} -b /:/host-rootfs -b /tmp -r ${ROOTFS} readlink /tmp/${LINK_NAME1} | grep ^/tmp/ced-host$
${PROOT} -b /:/host-rootfs -b /tmp:/foo -r ${ROOTFS} readlink /foo/${LINK_NAME1} | grep ^/foo/ced-host$
${PROOT} -b /:/host-rootfs -b /tmp -r ${ROOTFS} readlink /host-rootfs/tmp/${LINK_NAME1} | grep ^/tmp/ced-host$
${PROOT} -b /:/host-rootfs -b /tmp:/foo -r ${ROOTFS} readlink /host-rootfs/tmp/${LINK_NAME1} | grep ^/foo/ced-host$

rm /tmp/${LINK_NAME1}
rm ${ROOTFS}/tmp/${LINK_NAME1}

${PROOT} -b /:/host-rootfs -b /tmp -w /bin -r ${ROOTFS} symlink /bin/bar /bin/${LINK_NAME1}
${PROOT} -b /:/host-rootfs -b /tmp -w /bin -r ${ROOTFS} readlink ${LINK_NAME1} | grep ^/bin/bar$
rm ${ROOTFS}/bin/${LINK_NAME1}

${PROOT} -b /:/host-rootfs -b /tmp -w /tmp -r ${ROOTFS} symlink /bin/bar /tmp/${LINK_NAME1}
${PROOT} -b /:/host-rootfs -b /tmp -w /tmp -r ${ROOTFS} readlink ${LINK_NAME1} | grep ^/bin/bar$
${PROOT} -b /:/host-rootfs -b /tmp:/foo -w /foo -r ${ROOTFS} symlink /foo/bar /foo/${LINK_NAME2}
${PROOT} -b /:/host-rootfs -b /tmp:/foo -w /foo -r ${ROOTFS} readlink ${LINK_NAME2} | grep ^/foo/bar$
${PROOT} -b /:/host-rootfs -b /tmp -w /host-rootfs/tmp -r ${ROOTFS} readlink ${LINK_NAME2} | grep ^/foo/bar$
rm /tmp/${LINK_NAME2}
rm /tmp/${LINK_NAME1}
