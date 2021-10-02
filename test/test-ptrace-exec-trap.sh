if [ -z `which mcookie` ] || [ -z `which cmp` ] || [ -z `which rm` ] || [ ! -x  ${ROOTFS}/bin/ptrace-3 ] || [ ! -x  ${ROOTFS}/bin/true ]; then
    exit 125;
fi

TMP1=/tmp/$(mcookie)
TMP2=/tmp/$(mcookie)

${ROOTFS}/bin/ptrace-3 ${ROOTFS}/bin/true 2>&1 >${TMP1}

${PROOT} ${ROOTFS}/bin/ptrace-3 ${ROOTFS}/bin/true 2>&1 >${TMP2}

cmp ${TMP1} ${TMP2}

PTRACER_BEHAVIOR_1=1 ${ROOTFS}/bin/ptrace-3 ${ROOTFS}/bin/true 2>&1 >${TMP1}

PTRACER_BEHAVIOR_1=1 ${PROOT} ${ROOTFS}/bin/ptrace-3 ${ROOTFS}/bin/true 2>&1 >${TMP2}

cmp ${TMP1} ${TMP2}

PTRACER_BEHAVIOR_2=1 ${ROOTFS}/bin/ptrace-3 ${ROOTFS}/bin/true 2>&1 >${TMP1}

PTRACER_BEHAVIOR_2=1 ${PROOT} ${ROOTFS}/bin/ptrace-3 ${ROOTFS}/bin/true 2>&1 >${TMP2}

cmp ${TMP1} ${TMP2}

rm -f ${TMP1} ${TMP2}
