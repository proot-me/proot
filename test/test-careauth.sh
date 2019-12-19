if [ -z `which env` ] || [ -z `which rm` ] || [ -z `which mcookie` ] || [ -z `which true` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi
unset PROOT

TMP=$(mcookie)

cd /tmp
env ICEAUTHORITY= ${CARE} -o ${TMP} true
${CARE} -x ./${TMP}
./${TMP}/re-execute.sh
rm -fr ${TMP}

env XAUTHORITY= ${CARE} -o ${TMP} true
${CARE} -x ./${TMP}
./${TMP}/re-execute.sh
rm -fr ${TMP}

${CARE} -o ${TMP} -p ../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../ true
${CARE} -x ./${TMP}
./${TMP}/re-execute.sh
rm -fr ${TMP}
