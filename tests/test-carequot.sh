if [ -z `which env` ] || [ -z `which rm` ] || [ -z `which mcookie` ] || [ -z `which true` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi
unset PROOT

TMP=/tmp/$(mcookie)

env 'COMP_WORDBREAKS= 	
"'\''><;|&(:' ${CARE} -o ${TMP}.bin true

cd /tmp; ${CARE} -x ${TMP}.bin
${TMP}/re-execute.sh

rm -fr ${TMP}.bin ${TMP}
