if [ -z `which id` ] || [ -z `which grep` ] || [ ! -x ${ROOTFS}/bin/exec-m32-suid ] || [ ! -x ${ROOTFS}/bin/exec-m32-sgid ] || [ ! -x ${ROOTFS}/bin/getresuid ]  || [ ! -x ${ROOTFS}/bin/getresgid ]; then
    exit 125
fi

MY_UID=$(id -u)
MY_GID=$(id -g)

${PROOT} -i ${MY_UID}:${MY_GID} -R ${ROOTFS} exec-m32-suid /bin/getresuid | grep "^${MY_UID} 0 0$"

${PROOT} -i ${MY_UID}:${MY_GID} -R ${ROOTFS} exec-m32-suid /bin/getresgid | grep "^${MY_GID} ${MY_GID} ${MY_GID}$"

${PROOT} -i ${MY_UID}:${MY_GID} -R ${ROOTFS} exec-m32-sgid /bin/getresuid | grep "^${MY_UID} ${MY_UID} ${MY_UID}$"

${PROOT} -i ${MY_UID}:${MY_GID} -R ${ROOTFS} exec-m32-sgid /bin/getresgid | grep "^${MY_GID} 0 0$"

${PROOT} -R ${ROOTFS} exec-m32-suid /bin/getresuid | grep "^${MY_UID} ${MY_UID} ${MY_UID}$"

${PROOT} -R ${ROOTFS} exec-m32-suid /bin/getresgid | grep "^${MY_GID} ${MY_GID} ${MY_GID}$"

${PROOT} -R ${ROOTFS} exec-m32-sgid /bin/getresuid | grep "^${MY_UID} ${MY_UID} ${MY_UID}$"

${PROOT} -R ${ROOTFS} exec-m32-sgid /bin/getresgid | grep "^${MY_GID} ${MY_GID} ${MY_GID}$"

