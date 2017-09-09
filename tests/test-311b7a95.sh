if [ -z `which mcookie` ] || [ -z `which rm` ] || [ -z `which chmod` ] || [ -z `which echo` ]; then
    exit 125;
fi

TMP1=/tmp/$(mcookie)
TMP2=/tmp/$(mcookie)

echo "#! ${TMP2} -a"       > ${TMP1}
echo "#! $(which echo) -b" > ${TMP2}

chmod +x ${TMP1} ${TMP2}

RESULT=$(${PROOT} ${TMP1})
EXPECTED=$(${TMP1})

test "${RESULT}" = "${EXPECTED}"

rm -f ${TMP1} ${TMP2}
