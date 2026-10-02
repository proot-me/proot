if [ -z `which mcookie` ] || [ -z `which echo` ] || [ -z `which rm` ] || [ -z `which touch` ] || [ -z `which chmod` ] || [ -z `which grep` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
TMP2=/tmp/$(mcookie)

rm -f ${TMP}
touch ${TMP}
chmod +x ${TMP}

# Valgrind prepends "/bin/sh" in front of foreign binaries and uses
# LD_PRELOAD.
if $(echo ${PROOT} | grep -q valgrind); then
    ENV=$(which env)
    PROOT="env PROOT_FORCE_FOREIGN_BINARY=1 ${PROOT}"
    COMMAND1="-E LD_PRELOAD=.* -0 /bin/sh /bin/sh ${TMP}"
    TEST1="-- -U LD_LIBRARY_PATH -E LD_PRELOAD=.* -0 env ${ENV} LD_LIBRARY_PATH=test1 ${TMP}"
    TEST2="-- -E LD_PRELOAD=.* -E LD_LIBRARY_PATH=test2 -0 /bin/sh /bin/sh ${TMP}"
    TEST3="-- -E LD_PRELOAD=.* -E LD_LIBRARY_PATH=test2 -0 env ${ENV} LD_LIBRARY_PATH=test1 ${TMP}"
    TEST4="-- -U LD_LIBRARY_PATH -E LD_PRELOAD=.* -0 env ${ENV} LD_TRACE_LOADED_OBJECTS=1 ${TMP}"
    TEST5="-- -E LD_PRELOAD= -E LD_LIBRARY_PATH=test5 -0 sh /bin/sh -c ${TMP}"
    TEST52="-- -E LD_PRELOAD= -E LD_LIBRARY_PATH=test5 -0 sh /bin/sh -c sh -c ${TMP}"
    TEST6="-- -E LD_PRELOAD= -E LD_LIBRARY_PATH=test5 -0 env ${ENV} LD_LIBRARY_PATH=test6 ${TMP}"
    COMMAND2="-E LD_PRELOAD=.* -0 ${TMP} ${TMP} ${TMP2}"
else
    COMMAND1="-0 ${TMP} ${TMP}"
    TEST1="-- -E LD_LIBRARY_PATH=test1 ${COMMAND1}"
    TEST2="-- -E LD_LIBRARY_PATH=test2 ${COMMAND1}"
    TEST3="${TEST1}"
    TEST4="-- -E LD_TRACE_LOADED_OBJECTS=1 -E LD_LIBRARY_PATH=.+ ${COMMAND1}"
    TEST5="-- -E LD_LIBRARY_PATH=test5 ${COMMAND1}"
    TEST52=${TEST5}
    TEST6="-- -E LD_LIBRARY_PATH=test6 ${COMMAND1}"
    COMMAND2="-0 ${TMP} ${TMP} ${TMP2}"
fi

  ${PROOT} -q true ${TMP}
! ${PROOT} -q false ${TMP}
[ $? -eq 0 ]

  (cd /; ${PROOT} -q ./$(which true) ${TMP})
! (cd /; ${PROOT} -q ./$(which false) ${TMP})
[ $? -eq 0 ]

HOST_LD_LIBRARY_PATH=$(${PROOT} -q 'echo --' env | grep LD_LIBRARY_PATH)
test ! -z "${HOST_LD_LIBRARY_PATH}"

unset LD_LIBRARY_PATH
${PROOT} -q 'echo --' ${TMP} | grep -- "^-- -U LD_LIBRARY_PATH ${COMMAND1}$"
${PROOT} -q 'echo --' env LD_LIBRARY_PATH=test1 ${TMP} | grep -- "^${TEST1}$"
env LD_LIBRARY_PATH=test2 ${PROOT} -q 'echo --' ${TMP} | grep -- "^${TEST2}$"

env LD_LIBRARY_PATH=test2 ${PROOT} -q 'echo --' env LD_LIBRARY_PATH=test1 ${TMP} | grep -- "^${TEST3}$"

${PROOT} -q 'echo --' env LD_TRACE_LOADED_OBJECTS=1 ${TMP} | grep -E -- "^${TEST4}$"

env LD_LIBRARY_PATH=test5 ${PROOT} -q 'echo --' sh -c ${TMP} | grep -- "^${TEST5}$"
env LD_LIBRARY_PATH=test5 ${PROOT} -q 'echo --' sh -c "sh -c ${TMP}" | grep -- "^${TEST52}$"
env LD_LIBRARY_PATH=test5 ${PROOT} -q 'echo --' env LD_LIBRARY_PATH=test6 ${TMP} | grep -- "^${TEST6}$"

rm -f ${TMP2}
echo "#!${TMP}" > ${TMP2}
chmod +x ${TMP2}
${PROOT} -q 'echo --' ${TMP2} | grep -- "^-- -U LD_LIBRARY_PATH ${COMMAND2}$"

rm -fr ${TMP} ${TMP2}
