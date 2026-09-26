#!/bin/sh
# shellcheck disable=SC2086,SC2181 # $PROOT stays unquoted (memcheck prefixes it with valgrind), and $? is read after negated commands.
if [ -z "$(which mcookie)" ] || [ -z "$(which env)" ] || [ -z "$(which mkdir)" ] || [ -z "$(which rm)" ] || [ ! -x "${ROOTFS}/bin/readdir" ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)
mkdir "${TMP}"

! env PROOT_DONT_POLLUTE_ROOTFS=1 ${PROOT} -b "/bin:${TMP}/dont/create" "${ROOTFS}/bin/readdir" "${TMP}" | grep -w dont
[ $? -eq 0 ]

env PROOT_DONT_POLLUTE_ROOTFS=1 ${PROOT} -b "/bin:${TMP}/dont/create" sh -c "test -e ${TMP}/dont"

${PROOT} -b "/bin:${TMP}/dont/create" sh -c "test -e ${TMP}/dont"

! test -e "${TMP}/dont"
[ $? -eq 0 ]

chmod +rx -R "${TMP}"
rm -fr "${TMP}"
