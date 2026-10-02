if [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which chmod` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)

mkdir ${TMP}
chmod a-x ${TMP}
! ${PROOT} sh -c "cd $TMP"
[ $? -eq 0 ]

chmod a+x ${TMP}
rm -fr ${TMP}
