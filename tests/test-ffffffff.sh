if [ -z `which mcookie` ] || [ -z `which touch` ] || [ -z `which stat` ] || [ -z `which grep` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
touch ${TMP}

${PROOT} -0 stat -c %u:%g ${TMP} | grep 0:0

rm ${TMP}
