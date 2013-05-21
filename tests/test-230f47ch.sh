if [ -z `which id` ] || [ -z `which uname` ] || [ -z `which grep` ]; then
    exit 125;
fi

! ${PROOT} ${PROOT_RAW} /bin/true
if [ $? -eq 0 ]; then
    exit 125;
fi

export PROOT_NO_SECCOMP=1

${PROOT} ${PROOT_RAW} -0 id -u                 | grep ^0$
${PROOT} ${PROOT_RAW} -k 3.33.333 uname -r     | grep ^3\.33\.333$

${PROOT} -0       ${PROOT_RAW} id -u           | grep ^0$
${PROOT} -k 3.33.333 ${PROOT_RAW} uname -r     | grep ^3\.33\.333$

${PROOT} -0 ${PROOT_RAW} -k 3.33.333 id -u     | grep ^0$
${PROOT} -0 ${PROOT_RAW} -k 3.33.333 uname -r  | grep ^3\.33\.333$

${PROOT} -k 3.33.333 ${PROOT_RAW} -0 id -u     | grep ^0$
${PROOT} -k 3.33.333 ${PROOT_RAW} -0 uname -r  | grep ^3\.33\.333$
