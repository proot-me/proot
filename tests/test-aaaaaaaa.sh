if [ ! -x ${ROOTFS}/bin/true ] || [ -z `which id` ]; then
    exit 125;
fi

if [ `id -u` == 0 ]; then
    exit 125;
fi

DONT_EXIST="/a2516c0480c3bd4fa3548e17123a11e2"

${PROOT} ${ROOTFS} true

! ${PROOT} ${DONT_EXIST} true
[ $? -eq 0 ]

${PROOT} -r ${ROOTFS} true
${PROOT} -r ${DONT_EXIST} -r ${ROOTFS} true

! ${PROOT} -r ${ROOTFS} -r ${DONT_EXIST} true
[ $? -eq 0 ]

! ${PROOT} -r ${DONT_EXIST} ${ROOTFS} true
[ $? -eq 0 ]

! ${PROOT} ${ROOTFS} -r ${ROOTFS} true
[ $? -eq 0 ]

! ${PROOT} -v
[ $? -eq 0 ]

${PROOT} -b /bin/true:${DONT_EXIST} ${DONT_EXIST}

${PROOT} -r / -b /etc:/ true
${PROOT} -b /etc:/ true
