if [ ! -x  ${ROOTFS}/bin/pwd ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} -m /:/hostfs -w /hostfs/etc -r ${ROOTFS} pwd | grep '^/hostfs/etc$'
${PROOT} -m /:/hostfs -w /hostfs -r ${ROOTFS} pwd | grep '^/hostfs$'
${PROOT} -m /:/hostfs -w / -r ${ROOTFS} pwd | grep '^/$'
