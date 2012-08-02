if [ ! -x ${ROOTFS}/bin/readdir ] || [ ! -x ${ROOTFS}/bin/cat ] || [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which chmod` ]  || [ -z `which grep` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP1=/tmp/$(mcookie)
TMP2=/tmp/$(mcookie)
TMP3=$(mcookie)
TMP4=$(mcookie)

echo "content of ${TMP1}" > ${TMP1}

mkdir ${ROOTFS}/${TMP2}
chmod -rw ${ROOTFS}/${TMP2}

export LANG=C
! ${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} readdir ${TMP2}           | grep '^opendir(3): Permission denied$'
${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} readdir ${TMP2}/${TMP3}     | grep "^DT_REG  ${TMP4}$"
${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} cat ${TMP2}/${TMP3}/${TMP4} | grep "^content of ${TMP1}$"
${PROOT} -v -1 -r ${ROOTFS} -b /tmp:${TMP2}/${TMP3}/${TMP4} readdir ${TMP2}/${TMP3}        | grep "^DT_DIR  ${TMP4}$"
# TODO ${PROOT} -v -1 -r ${ROOTFS} -b /tmp:${TMP2}/${TMP3}/${TMP4} readdir /tmp            | grep "DT_DIR  ${TMP2}$"
# TODO ${PROOT} -v -1 -r ${ROOTFS} -b /tmp:/${TMP4} readdir /                              | grep "DT_REG  ${TMP4}$"

! test -e ${ROOTFS}/${TMP2}/${TMP3}
! test -e ${ROOTFS}/${TMP2}/${TMP3}/${TMP4}

rm -fr ${ROOTFS}/${TMP2}

mkdir ${TMP2}
chmod -rw ${TMP2}

export LANG=C
! ${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/readdir ${TMP2}           | grep '^opendir(3): Permission denied$'
${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/readdir ${TMP2}/${TMP3}     | grep "^DT_REG  ${TMP4}$"
${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/cat ${TMP2}/${TMP3}/${TMP4} | grep "^content of ${TMP1}$"
${PROOT} -v -1 -b /tmp:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/readdir ${TMP2}/${TMP3}        | grep "^DT_DIR  ${TMP4}$"
# TODO ${PROOT} -v -1 -b /tmp:${TMP2}/${TMP3}/${TMP4} readdir /tmp                          | grep "DT_DIR  ${TMP2}$"
# TODO ${PROOT} -v -1 -b /tmp:/${TMP4} readdir /                                            | grep "DT_REG  ${TMP4}$"

! test -e /${TMP2}/${TMP3}
! test -e /${TMP2}/${TMP3}/${TMP4}

rm -fr /${TMP2}
