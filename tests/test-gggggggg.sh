if [ -z `which mcookie` ] || [ -z `which true` ] || [ -z `which mkdir` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir ${TMP}

env PROOT_DONT_POLLUTE_ROOTFS=1 ${PROOT} -b /bin:${TMP}/do/not/create -b /bin:${TMP}/dont/create true
test ! -e ${TMP}/do
test ! -e ${TMP}/dont

${PROOT} -b /bin:${TMP}/do/create -b /bin:${TMP}/dont/create true
test -e ${TMP}/do
test -e ${TMP}/dont

chmod +rx -R ${TMP}
rm -fr ${TMP}
