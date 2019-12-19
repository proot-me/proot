if [ -z `which mcookie` ] || [ -z `which mkdir` ] || [ -z `which cat` ] || [ -z `which grep` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP1=/tmp/`mcookie`
TMP2=/tmp/`mcookie`
TMP3=/tmp/`mcookie`

# /a/b/c
# /a/b
# /a/d
# /a

echo 'binding 1' > ${TMP1}
echo 'binding 2' > ${TMP2}

mkdir -p ${TMP3}/a/b

BINDINGS="-b ${TMP1}:${TMP3}/a/b/c -b ${TMP3}/a/b -b ${TMP2}:${TMP3}/a/d -b ${TMP3}/a"
${PROOT} ${BINDINGS} cat ${TMP3}/a/b/c | grep '^binding 1$'

BINDINGS="-b ${TMP3}/a -b ${TMP2}:${TMP3}/a/d -b ${TMP3}/a/b -b ${TMP1}:${TMP3}/a/b/c"
${PROOT} ${BINDINGS} cat ${TMP3}/a/d   | grep '^binding 2$'

mkdir -p ${TMP3}/c/b

# /c/b/a
# /c/b
# /c/d
# /c

BINDINGS="-b ${TMP1}:${TMP3}/c/b/a -b ${TMP3}/c/b -b ${TMP2}:${TMP3}/c/d -b ${TMP3}/c"
${PROOT} ${BINDINGS} cat ${TMP3}/c/b/a | grep '^binding 1$'

BINDINGS="-b ${TMP3}/c -b ${TMP2}:${TMP3}/c/d -b ${TMP3}/c/b -b ${TMP1}:${TMP3}/c/b/a"
${PROOT} ${BINDINGS} cat ${TMP3}/c/d | grep '^binding 2$'

rm ${TMP1}
rm ${TMP2}
rm -fr ${TMP3}
