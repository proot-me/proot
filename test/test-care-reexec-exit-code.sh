if [ -z `which cpio` ] || [ -z `which rm` ] || [ -z `which mcookie` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi
unset PROOT

TMP=/tmp/$(mcookie)

${CARE} -o ${TMP}.cpio sh -c 'exit 0'

cd /tmp
cpio -idmuvF ${TMP}.cpio

${TMP}/re-execute.sh

set +e
${TMP}/re-execute.sh sh -c 'exit 132'
status=$?
set -e
[ $status -eq 132 ]

rm -f ${TMP}.cpio
