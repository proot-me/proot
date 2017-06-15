if [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which test` ] || [ -z `which grep` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir ${TMP}

${PROOT} -b `which true`:${TMP}/true `which true`
! test -e ${TMP}/true
[ $? -eq 0 ]

rm -fr ${TMP}
