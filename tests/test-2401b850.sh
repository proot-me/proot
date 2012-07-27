if [ -z `which mcookie` ] || [ -z `which echo` ] || [ -z `which rm` ]  || [ -z `which touch` ] || [ -z `which chmod` ] || [ -z `which grep` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
TMP2=/tmp/$(mcookie)

rm -f ${TMP}
touch ${TMP}
chmod +x ${TMP}

  ${PROOT} -q true / ${TMP}
! ${PROOT} -q false / ${TMP}
[ $? -eq 0 ]

HOST_LD_LIBRARY_PATH=$(${PROOT} -q 'echo --' / env | grep LD_LIBRARY_PATH)
test ! -z ${HOST_LD_LIBRARY_PATH}

unset LD_LIBRARY_PATH
${PROOT} -q 'echo --' / ${TMP} | grep -- "^-- -U LD_LIBRARY_PATH -0 ${TMP} ${TMP}$"
${PROOT} -q 'echo --' / env LD_LIBRARY_PATH=test1 ${TMP} | grep -- "^-- -E LD_LIBRARY_PATH=test1 -0 ${TMP} ${TMP}$"
env LD_LIBRARY_PATH=test2 ${PROOT} -q 'echo --' / ${TMP} | grep -- "^-- -E LD_LIBRARY_PATH=test2 -0 ${TMP} ${TMP}$"

env LD_LIBRARY_PATH=test2 ${PROOT} -q 'echo --' / env LD_LIBRARY_PATH=test1 ${TMP} | grep -- "^-- -E LD_LIBRARY_PATH=test1 -0 ${TMP} ${TMP}$"

${PROOT} -q 'echo --' / env LD_TRACE_LOADED_OBJECTS=1 ${TMP} | grep -E -- "^-- -E LD_TRACE_LOADED_OBJECTS=1 -E LD_LIBRARY_PATH=.+ -0 ${TMP} ${TMP}$"

rm -f ${TMP2}
echo "#!${TMP}" > ${TMP2}
chmod +x ${TMP2}
${PROOT} -q 'echo --' ${TMP2} | grep -- "^-- -U LD_LIBRARY_PATH -0 ${TMP} ${TMP} ${TMP2}$"

rm -fr ${TMP} ${TMP2}
