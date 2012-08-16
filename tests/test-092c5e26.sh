if [ ! -x  ${ROOTFS}/bin/echo ] ||  [ ! -x  ${ROOTFS}/bin/argv0 ] || [ -z `which mcookie` ] || [ -z `which grep` ] || [ -z `which cat` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=$(mcookie)
TMP_ABS=/tmp/${TMP}

${PROOT} -r ${ROOTFS} argv0      | grep '^argv0$'
${PROOT} -r ${ROOTFS} /bin/argv0 | grep '^/bin/argv0$'

cat > ${ROOTFS}/${TMP_ABS} <<EOF
#!/bin/argv0
test
EOF
chmod +x ${ROOTFS}/${TMP_ABS}

env PATH=/tmp ${PROOT} -r ${ROOTFS} ${TMP} | grep '^/bin/argv0$'
${PROOT} -r ${ROOTFS} ${TMP_ABS}           | grep '^/bin/argv0$'

# Valgrind uses LD_PRELOAD.
if $(echo ${PROOT} | grep -q valgrind); then
    EXTRA='-E LD_PRELOAD=.*'
fi

unset LD_LIBRARY_PATH

env PROOT_FORCE_FOREIGN_BINARY=1 PATH=/tmp:/bin:/usr/bin ${PROOT} -r ${ROOTFS} -q echo ${TMP} | grep "^-U LD_LIBRARY_PATH ${EXTRA}-0 /bin/argv0 /bin/argv0 ${TMP_ABS}$"
env PROOT_FORCE_FOREIGN_BINARY=1 ${PROOT} -r ${ROOTFS} -q echo ${TMP_ABS} | grep "^-U LD_LIBRARY_PATH ${EXTRA}-0 /bin/argv0 /bin/argv0 ${TMP_ABS}$"

cat > ${ROOTFS}/${TMP_ABS} <<EOF
FOREIGN BINARY FORMAT
EOF
chmod +x ${ROOTFS}/${TMP_ABS}

# Valgrind prepends "/bin/sh" in front of foreign binaries.
if ! $(echo ${PROOT} | grep -q valgrind); then
    env PATH=/tmp:/bin:/usr/bin ${PROOT} -r ${ROOTFS} -q echo ${TMP} | grep "^-U LD_LIBRARY_PATH -0 ${TMP} ${TMP_ABS}$"
    ${PROOT} -r ${ROOTFS} -q echo ${TMP_ABS}                | grep "^-U LD_LIBRARY_PATH -0 ${TMP_ABS} ${TMP_ABS}$"
fi

rm -f ${TMP_ABS}
