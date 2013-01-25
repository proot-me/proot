if [ -z `which mcookie` ] || [ -z `which true` ] || [ -z `which mkdir` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir ${TMP}

env PROOT_DONT_POLLUTE_ROOTFS=1 ${PROOT} -b /bin:${TMP}/do/not/create true
test ! -e ${TMP}/do

${PROOT} -b /bin:${TMP}/do/create true
test -e ${TMP}/do

rm -r ${TMP}
