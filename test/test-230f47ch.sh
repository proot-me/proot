if [ -z `which id` ] || [ -z `which uname` ] || [ -z `which grep` ]; then
    exit 125;
fi

! ${PROOT} ${PROOT_RAW} /bin/true
if [ $? -eq 0 ]; then
    exit 125;
fi

kver=$(uname -r)

${PROOT} ${PROOT_RAW} -0 id -u                    | grep ^0$
${PROOT} ${PROOT_RAW} -i 123:456 id -u            | grep ^123$
${PROOT} ${PROOT_RAW} -k $kver-3.33.333 uname -r  | grep ^.*-3\.33\.333$

${PROOT} -0       ${PROOT_RAW} id -u              | grep ^0$
${PROOT} -i 123:456 ${PROOT_RAW} id -u            | grep ^123$
${PROOT} -k $kver-3.33.333 ${PROOT_RAW} uname -r  | grep ^.*-3\.33\.333$

${PROOT} -0 ${PROOT_RAW} -k $kver-3.33.333 id -u     | grep ^0$
${PROOT} -0 ${PROOT_RAW} -k $kver-3.33.333 uname -r  | grep ^.*-3\.33\.333$

${PROOT} -k $kver-3.33.333 ${PROOT_RAW} -0 id -u     | grep ^0$
${PROOT} -k $kver-3.33.333 ${PROOT_RAW} -0 uname -r  | grep ^.*-3\.33\.333$

${PROOT} -i 123:456 ${PROOT_RAW} -k $kver-3.33.333 id -u | grep ^123$
${PROOT} -k $kver-3.33.333 ${PROOT_RAW} -i 123:456 id -u | grep ^123$
