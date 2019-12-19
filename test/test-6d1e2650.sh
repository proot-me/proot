if [ ! -x  ${ROOTFS}/bin/true ] || [ -z `which env` ]; then
    exit 125;
fi

! env PATH=/nib ${PROOT} -r ${ROOTFS} true
[ $? -eq 0 ]

env PATH=/bin ${PROOT} -r ${ROOTFS} true
