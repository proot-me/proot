if [ -z `which echo` ] || [ -z `which rm` ]  || [ -z `which touch` ] || [ -z `which chmod` ] || [ -z `which grep` ]; then
    exit 125;
fi

TMP=/tmp/ac8a16ac292c6bf4e37d298a7d23a34a

rm -f ${TMP}
touch ${TMP}
chmod +x ${TMP}

  ${PROOT} -q true / ${TMP}
! ${PROOT} -q false / ${TMP}

HOST_LD_LIBRARY_PATH=$(${PROOT} -q echo / env | grep LD_LIBRARY_PATH)
test ! -z ${HOST_LD_LIBRARY_PATH}

unset LD_LIBRARY_PATH
${PROOT} -q echo / ${TMP} | grep -- "^-0 ${TMP} -U LD_LIBRARY_PATH ${TMP}$"
${PROOT} -q echo / env LD_LIBRARY_PATH=test1 ${TMP} | grep -- "^-0 ${TMP} -E LD_LIBRARY_PATH=test1 ${TMP}$"
env LD_LIBRARY_PATH=test2 ${PROOT} -q echo / ${TMP} | grep -- "^-0 ${TMP} -E LD_LIBRARY_PATH=test2 ${TMP}$"

env LD_LIBRARY_PATH=test2 ${PROOT} -q echo / env LD_LIBRARY_PATH=test1 ${TMP} | grep -- "^-0 ${TMP} -E LD_LIBRARY_PATH=test1 ${TMP}$"

${PROOT} -q echo / env LD_TRACE_LOADED_OBJECTS=1 ${TMP} | grep -E -- "^-0 ${TMP} -E LD_TRACE_LOADED_OBJECTS=1 -E LD_LIBRARY_PATH=.+ ${TMP}$"
