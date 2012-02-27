if [ ! -x  ${ROOTFS}/bin/true ] || [ -z `which env` ]; then
    exit 125;
fi

! env PATH=/nib ${PROOT} ${ROOTFS} true
env PATH=/bin ${PROOT} ${ROOTFS} true
