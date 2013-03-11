if [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which stat` ] || [ -z `which grep` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir ${TMP}

${PROOT} -b /bin/true:${TMP}/true /bin/true
stat -c %a ${TMP}/true | grep '^0$'

rm -fr ${TMP}
