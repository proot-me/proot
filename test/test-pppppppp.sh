if [ -z `which mcookie` ] || [ -z `which true` ] || [ -z `which mkdir` ]|| [ -z `which env` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir -p ${TMP}/true

! ${PROOT} true
if [ $? -eq 0 ]; then
    exit 125;
fi

env PATH=${TMP}:${PATH} ${PROOT} true

env PATH=${TMP}:${PATH} ${PROOT} env true

rm -fr ${TMP}

