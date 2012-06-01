if [ -z `which rm` ] || [ -z `which mcookie` ]; then
    exit 125;
fi

TMP=`mcookie`
rm -f ${TMP}
WHICH_RM=`which rm`

COMMAND="${PROOT} / rm ${TMP}"

set +e
WITHOUT=$(${COMMAND} 2>&1)
WITH=$(env PROOT_IGNORE_ELF_INTERPRETER=1 ${COMMAND} 2>&1)
set -e

echo ${WITHOUT} | grep ^${WHICH_RM}
echo ${WITH}    | grep -v ^${WHICH_RM}
