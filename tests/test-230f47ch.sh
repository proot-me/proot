if [ -z `which id` ] || [ -z `which uname` ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} ${PROOT_RAW} -0 id -u              | grep ^0$
${PROOT} ${PROOT_RAW} -k 1.2.3 uname -r     | grep ^1.2.3$

${PROOT} -0       ${PROOT_RAW} id -u        | grep ^0$
${PROOT} -k 1.2.3 ${PROOT_RAW} uname -r     | grep ^1.2.3$

${PROOT} -0 ${PROOT_RAW} -k 1.2.3 id -u     | grep ^0$
${PROOT} -0 ${PROOT_RAW} -k 1.2.3 uname -r  | grep ^1.2.3$

${PROOT} -k 1.2.3 ${PROOT_RAW} -0 id -u     | grep ^0$
${PROOT} -k 1.2.3 ${PROOT_RAW} -0 uname -r  | grep ^1.2.3$
