if [ ! -x  ${ROOTFS}/bin/argv ] || [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which chmod` ] || [ -z `which env` ] || [ -z `which rm` ] || [ -z `which grep` ] || [ -z `which env` ] || [ -z `which ln` ]; then
    exit 125;
fi

BIN_DIR=/tmp/$(mcookie)
TMP=$(mcookie)
TMP2=$(mcookie)

mkdir ${BIN_DIR}
echo "#! ${ROOTFS}/bin/argv -x" > ${BIN_DIR}/${TMP}
chmod +x ${BIN_DIR}/${TMP}
ln -s ${BIN_DIR}/${TMP} ${BIN_DIR}/${TMP2}

${PROOT} env ${BIN_DIR}/${TMP} | grep "${ROOTFS}/bin/argv -x ${BIN_DIR}/${TMP}"

${PROOT} env PATH=${BIN_DIR} ${TMP} | grep "${ROOTFS}/bin/argv -x ${BIN_DIR}/${TMP}"

(cd ${BIN_DIR}; ${PROOT} env ./${TMP}) | grep "${ROOTFS}/bin/argv -x ./${TMP}"

${PROOT} env ${BIN_DIR}/${TMP2} | grep "${ROOTFS}/bin/argv -x ${BIN_DIR}/${TMP2}"

${PROOT} env PATH=${BIN_DIR} ${TMP2} | grep "${ROOTFS}/bin/argv -x ${BIN_DIR}/${TMP2}"

(cd ${BIN_DIR}; ${PROOT} env ./${TMP2}) | grep "${ROOTFS}/bin/argv -x ./${TMP2}"

${PROOT} ${BIN_DIR}/${TMP} | grep "${ROOTFS}/bin/argv -x ${BIN_DIR}/${TMP}"

env PATH=${BIN_DIR} ${PROOT} ${TMP} | grep "${ROOTFS}/bin/argv -x ${BIN_DIR}/${TMP}"

# TODO: (cd ${BIN_DIR}; ${PROOT} ./${TMP}) | grep "${ROOTFS}/bin/argv -x ./${TMP}"
(cd ${BIN_DIR}; ${PROOT} sh -c "true; ./${TMP}") | grep "${ROOTFS}/bin/argv -x ./${TMP}"

rm -fr ${BIN_DIR}
