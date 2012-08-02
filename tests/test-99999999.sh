if [ ! -x  ${ROOTFS}/bin/readlink ] || [ -z `which readlink` ] || [ -z `which cut` ] || [ -z `which grep` ] || [ -z `which md5sum` ]; then
    exit 125;
fi

WHICH_READLINK=$(readlink -f $(which readlink))

${PROOT} / readlink /proc/self/exe         | grep ^${WHICH_READLINK}$
${PROOT} / sh -c 'readlink /proc/self/exe' | grep ^${WHICH_READLINK}$
${PROOT} / bash -c 'readlink /proc/$$/exe'   | grep ^${WHICH_READLINK}$
${PROOT} -b /proc ${ROOTFS} readlink /proc/self/exe | grep ^/bin/readlink$

${PROOT} / readlink /proc/1/../self/exe         | grep ^${WHICH_READLINK}$
${PROOT} / sh -c 'readlink /proc/1/../self/exe' | grep ^${WHICH_READLINK}$
${PROOT} / bash -c 'readlink /proc/1/../$$/exe'   | grep ^${WHICH_READLINK}$
${PROOT} -b /proc ${ROOTFS} readlink /proc/1/../self/exe | grep ^/bin/readlink$

! ${PROOT} / readlink /proc/self/exe/
[ $? -eq 0 ]

! ${PROOT} / readlink /proc/self/exe/..
[ $? -eq 0 ]

! ${PROOT} / readlink /proc/self/exe/../exe
[ $? -eq 0 ]

! ${PROOT} -b /proc / readlink /proc/self/exe/
[ $? -eq 0 ]

! ${PROOT} -b /proc / readlink /proc/self/exe/..
[ $? -eq 0 ]

! ${PROOT} -b /proc / readlink /proc/self/exe/../exe
[ $? -eq 0 ]

TEST=$(${PROOT} / readlink /proc/self/fd/0 | grep -E "^/proc/[[:digit:]]+/fd/0$" | true)
test -z $TEST

TEST=$(${PROOT} -b /proc ${ROOTFS} readlink /proc/self/fd/0 | grep -E "^/proc/[[:digit:]]+/fd/0$" | true)
test -z $TEST

if [ ! -z $$ ]; then
TEST=$(readlink -f /proc/$$/exe)
${PROOT} / sh -c 'true; readlink /proc/$$/exe' | grep ${TEST}
fi

MD5=$(md5sum $(which md5sum) | cut -f 1 -d ' ')

MD5_PROOT=$(${PROOT} / md5sum /proc/self/exe | cut -f 1 -d ' ')
test ${MD5_PROOT} = ${MD5}

MD5_PROOT=$(${PROOT} -b /proc / md5sum /proc/self/exe | cut -f 1 -d ' ')
test ${MD5_PROOT} = ${MD5}
