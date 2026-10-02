if [ ! -x ${ROOTFS}/bin/true ] || [ -z `which env` ]; then
    exit 125;
fi

! ${PROOT} -r ${ROOTFS} /true
[ $? -eq 0 ]

! ${PROOT} -r ${ROOTFS} ./true
[ $? -eq 0 ]

! env PATH='' ${PROOT} -r ${ROOTFS} true
[ $? -eq 0 ]

! env PATH='' ${PROOT} -r ${ROOTFS} -w /bin true
[ $? -eq 0 ]

env PATH='' ${PROOT} -r ${ROOTFS} -w /bin ./true

env PATH='' ${PROOT} -r ${ROOTFS} -w / bin/true

env PATH='' ${PROOT} -r ${ROOTFS} -w / bin/./true

env PATH='' ${PROOT} -r ${ROOTFS} -w / ../bin/true

! env PATH='' ${PROOT} -r ${ROOTFS} -w /bin/true ../true
[ $? -eq 0 ]

! env --unset PATH ${PROOT} -r ${ROOTFS} true
[ $? -eq 0 ]

! env --unset PATH ${PROOT} -r ${ROOTFS} -w /bin true
[ $? -eq 0 ]

env --unset PATH ${PROOT} -r ${ROOTFS} -w /bin ./true

env --unset PATH ${PROOT} -r ${ROOTFS} -w / /bin/true

env --unset PATH ${PROOT} -r ${ROOTFS} -w / /bin/./true

env --unset PATH ${PROOT} -r ${ROOTFS} -w / ../bin/true

! env --unset PATH ${PROOT} -r ${ROOTFS} -w /bin/true ../true
[ $? -eq 0 ]
