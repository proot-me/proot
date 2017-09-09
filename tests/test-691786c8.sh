if [ ! -x /usr/bin/echo ] || [ -z `which mcookie` ] || [ -z `which chmod` ] || [ -z `which env` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)

echo '#!/usr/bin/echo XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX' > ${TMP}

chmod +x ${TMP}

RESULT=$(${PROOT} ${TMP})
EXPECTED=$(${TMP})

[ "${RESULT}" = "${EXPECTED}" ]

echo '#!//../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../usr/bin/echo XXXXXXXXX' > ${TMP}

RESULT=$(${PROOT} ${TMP})
EXPECTED=$(${TMP})

[ "${RESULT}" = "${EXPECTED}" ]
[ "${RESULT}" = "${TMP}" ]

echo '#!/../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../../usr/bin/echo XXXXXXXXX' > ${TMP}

! ${PROOT} ${TMP}
[ $? -eq 0 ]

! ${TMP}
[ $? -eq 0 ]

echo '#!                                                                                                                                                                                                        ' > ${TMP}

${PROOT} ${TMP}

echo '#!' > ${TMP}

${PROOT} ${TMP}

/usr/bin/echo "#!${TMP}" > ${TMP}

env LANG=C ${PROOT} ${TMP} 2>&1 | grep 'Too many levels of symbolic links'
[ $? -eq 0 ]

rm -f ${TMP}
