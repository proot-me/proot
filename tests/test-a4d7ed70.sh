if [ -z `which mcookie` ] || [ -z `which mkdir` ]  || [ -z `which ln` ] || [ -z `which ls` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir ${TMP}
ln -s /proc/self/fd ${TMP}/fd
ln -s ${TMP}/fd/0 ${TMP}/stdin

${PROOT} \ls ${TMP}/stdin | grep ^${TMP}/stdin$

rm -fr ${TMP}
