if [ ! -x /bin/true ] || [ -z `which grep` ] || [ -z `which env` ] || [ -z `which mcookie`]  || [ -z `which rm` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)

${CARE} -o ${TMP}/ env LD_SHOW_AUXV=1 true   | grep '^AT_HWCAP:[[:space:]]*0\?$'
${TMP}/re-execute.sh                         | grep '^AT_HWCAP:[[:space:]]*0\?$'
${TMP}/re-execute.sh env LD_SHOW_AUXV=1 true | grep '^AT_HWCAP:[[:space:]]*0\?$'

rm -fr ${TMP}
