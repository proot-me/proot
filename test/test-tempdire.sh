if [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which cp` ] || [ -z `which rm` ] || [ -z `which chmod` ] || [ -z `which env` ] || [ -z `which true` ]; then
    exit 125
fi

TMP=/tmp/$(mcookie)

mkdir ${TMP}

cp $(which true) ${TMP}/
if [ ! -x ${TMP}/true ]; then
   rm -fr ${TMP}
   exit 125
fi
rm -f ${TMP}/true

chmod -w ${TMP}

! env PROOT_TMP_DIR=${TMP} ${PROOT} true
[ $? -eq 0 ]

chmod +w ${TMP}
env PROOT_TMP_DIR=${TMP} ${PROOT} true

rm -fr ${TMP}
