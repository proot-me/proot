if [ ! -x ${ROOTFS}/bin/true ]; then
    exit 125;
fi

DONT_EXIST="/a2516c0480c3bd4fa3548e17123a11e2"

${PROOT} ${ROOTFS} true
! ${PROOT} ${DONT_EXIST} true

${PROOT} -r ${ROOTFS} true
${PROOT} -r ${DONT_EXIST} -r ${ROOTFS} true

! ${PROOT} -r ${ROOTFS} -r ${DONT_EXIST} true
! ${PROOT} -r ${DONT_EXIST} ${ROOTFS} true

! ${PROOT} ${ROOTFS} -r ${ROOTFS} true

! ${PROOT} -v

${PROOT} -b /bin/true:${DONT_EXIST} ${DONT_EXIST}

