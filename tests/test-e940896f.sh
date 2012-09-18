if [ ! -x ${ROOTFS}/bin/readdir ] || [ -z `which mcookie` ] || [ -z `which rm` ]  || [ -z `which mkdir` ] || [ -z `which chmod` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP1=/tmp/$(mcookie)
TMP2=${TMP1}/$(mcookie)/$(mcookie)

rm -fr ${TMP1}
rm -fr ${ROOTFS}/${TMP1}

mkdir -p ${TMP2}
mkdir -p ${ROOTFS}/${TMP1}
chmod -w ${ROOTFS}/${TMP1}

cd ${TMP2}
${PROOT} -r ${ROOTFS} -b . readdir ${TMP1}
${PROOT} -r ${ROOTFS} -b . readdir ${TMP2}
${PROOT} -r ${ROOTFS} -b . readdir ${TMP2}/..
${PROOT} -r ${ROOTFS} -b . readdir ${TMP2}/../..

rm -fr ${TMP1}
rm -fr ${ROOTFS}/${TMP1}
