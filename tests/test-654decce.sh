if [ ! -x ${ROOTFS}/bin/readdir ] || [ ! -x ${ROOTFS}/bin/cat ] || [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which chmod` ]  || [ -z `which grep` ] || [ -z `which rm` ] || [ -z `which id` ]; then
    exit 125;
fi

if [ `id -u` -eq 0 ]; then
    exit 125;
fi

TMP1=/tmp/$(mcookie)
TMP2=/tmp/$(mcookie)
TMP3=$(mcookie)
TMP4=$(mcookie)

echo "content of ${TMP1}" > ${TMP1}

mkdir -p ${ROOTFS}/${TMP2}
chmod -rw ${ROOTFS}/${TMP2}

export LANG=C
! ${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} readdir ${TMP2}           | grep '^opendir(3): Permission denied$'
${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} readdir ${TMP2}/${TMP3}     | grep "DT_REG  ${TMP4}"
${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} cat ${TMP2}/${TMP3}/${TMP4} | grep "^content of ${TMP1}$"
${PROOT} -v -1 -r ${ROOTFS} -b /tmp:${TMP2}/${TMP3}/${TMP4} readdir ${TMP2}/${TMP3}        | grep "DT_DIR  ${TMP4}"
# TODO ${PROOT} -v -1 -r ${ROOTFS} -b /tmp:${TMP2}/${TMP3}/${TMP4} readdir /tmp            | grep "DT_DIR  ${TMP2}"
# TODO ${PROOT} -v -1 -r ${ROOTFS} -b /tmp:/${TMP4} readdir /                              | grep "DT_REG  ${TMP4}"

${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} -b /etc/fstab:${TMP2}/${TMP3}/motd readdir ${TMP2}/${TMP3} | grep "DT_REG  ${TMP4}"
${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} -b /etc/fstab:${TMP2}/${TMP3}/motd readdir ${TMP2}/${TMP3} | grep "DT_REG  motd"

${PROOT} -v -1 -r ${ROOTFS} -b ${TMP1}:${TMP2}/${TMP3}/${TMP4}/motd -b /etc/fstab:${TMP2}/${TMP3}/motd cat ${TMP2}/${TMP3}/${TMP4}/motd | grep "^content of ${TMP1}$"
${PROOT} -v -1 -r ${ROOTFS} -b /etc/fstab:${TMP2}/${TMP3}/motd -b ${TMP1}:${TMP2}/${TMP3}/${TMP4}/motd cat ${TMP2}/${TMP3}/${TMP4}/motd | grep "^content of ${TMP1}$"

! chmod +rw ${ROOTFS}/${TMP2}
rm -fr ${ROOTFS}/${TMP2}

mkdir -p ${TMP2}
chmod -rw ${TMP2}

export LANG=C
! ${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/readdir ${TMP2}           | grep '^opendir(3): Permission denied$'
${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/readdir ${TMP2}/${TMP3}     | grep "DT_REG  ${TMP4}"
${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/cat ${TMP2}/${TMP3}/${TMP4} | grep "^content of ${TMP1}$"
${PROOT} -v -1 -b /tmp:${TMP2}/${TMP3}/${TMP4} ${ROOTFS}/bin/readdir ${TMP2}/${TMP3}        | grep "DT_DIR  ${TMP4}"
# TODO ${PROOT} -v -1 -b /tmp:${TMP2}/${TMP3}/${TMP4} readdir /tmp                          | grep "DT_DIR  ${TMP2}"
# TODO ${PROOT} -v -1 -b /tmp:/${TMP4} readdir /                                            | grep "DT_REG  ${TMP4}"

${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} -b /etc/fstab:${TMP2}/${TMP3}/motd ${ROOTFS}/bin/readdir ${TMP2}/${TMP3} | grep "DT_REG  ${TMP4}"
${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4} -b /etc/fstab:${TMP2}/${TMP3}/motd ${ROOTFS}/bin/readdir ${TMP2}/${TMP3} | grep "DT_REG  motd"

${PROOT} -v -1 -b ${TMP1}:${TMP2}/${TMP3}/${TMP4}/motd -b /etc/fstab:${TMP2}/${TMP3}/motd ${ROOTFS}/bin/cat ${TMP2}/${TMP3}/${TMP4}/motd | grep "^content of ${TMP1}$"
${PROOT} -v -1 -b /etc/fstab:${TMP2}/${TMP3}/motd -b ${TMP1}:${TMP2}/${TMP3}/${TMP4}/motd ${ROOTFS}/bin/cat ${TMP2}/${TMP3}/${TMP4}/motd | grep "^content of ${TMP1}$"

${PROOT} -b /bin:/this1/does/not/exist -b /tmp:/this2/does/not/exist ${ROOTFS}/bin/readdir /this1/
${PROOT} -b /bin:/this1/does/not/exist -b /tmp:/this2/does/not/exist ${ROOTFS}/bin/readdir /this2/
${PROOT} -b /tmp:/this1/does/not/exist -b /bin:/this2/does/not/exist ${ROOTFS}/bin/readdir /this1/
${PROOT} -b /tmp:/this1/does/not/exist -b /bin:/this2/does/not/exist ${ROOTFS}/bin/readdir /this2/

! chmod +rw ${TMP1} ${TMP2}
rm -fr ${TMP1} ${TMP2}
