if [ ! -x  ${ROOTFS}/bin/pwd ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} -m /:/hostfs -w /hostfs/bin ${ROOTFS} pwd | grep '^/hostfs/bin$'
${PROOT} -m /:/hostfs -w /hostfs ${ROOTFS} pwd | grep '^/hostfs$'
${PROOT} -m /:/hostfs -w / ${ROOTFS} pwd | grep '^/$'
