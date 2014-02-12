if [ ! -e /boot ] || [ -z `which mcookie` ] || [ -z `which ln` ] || [ -z `which mkdir` ] || [ -z `which rm` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi
unset PROOT

TMP_PROOT=/tmp/$(mcookie)
TMP_OUTPUT=/tmp/$(mcookie)/

ln -s ${CARE} ${TMP_PROOT}
mkdir ${TMP_OUTPUT}

${TMP_PROOT} -b /boot:/toob ${CARE} -o ${TMP_OUTPUT}/ ls /toob

test -e ${TMP_OUTPUT}/rootfs/toob

! test -e ${TMP_OUTPUT}/rootfs/boot
[ $? -eq 0 ]

${TMP_OUTPUT}/re-execute.sh

rm -fr ${TMP_PROOT} ${TMP_OUTPUT}


