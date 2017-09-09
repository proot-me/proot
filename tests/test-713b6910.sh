if [ -z `which mcookie` ] || [ -z `which rm` ] || [ -z `which cat` ] || [ -z `which chmod` ] || [ -z `which ln` ] || [ -z `which grep` ] || [ -z `which mkdir` ] || [ ! -x  ${ROOTFS}/bin/readlink ]; then
    exit 125;
fi

######################################################################

TMP1=/tmp/$(mcookie)
TMP2=/tmp/$(mcookie)
TMP3=/tmp/$(mcookie)
TMP4=/tmp/$(mcookie)

rm -fr ${TMP1} ${TMP2} ${TMP3} ${TMP4}

######################################################################

cat > ${TMP1} <<'EOF'
#!/bin/sh
echo $0
EOF

chmod +x ${TMP1}
ln -s ${TMP1} ${TMP2}

${PROOT} ${TMP2} | grep -v ${TMP1}
${PROOT} ${TMP2} | grep ${TMP2}

######################################################################

mkdir -p ${TMP3}
cd ${TMP3}

ln -s $(which true) false
! ${PROOT} false

echo "#!$(which false)" > true
chmod a-x true
${PROOT} true

######################################################################

ln -s ${ROOTFS}/bin/readlink ${TMP4}

TEST1=$(${PROOT} ${ROOTFS}/bin/readlink /proc/self/exe)
TEST2=$(${PROOT} ${TMP4} /proc/self/exe)

test "${TEST1}" = "${TEST2}"

######################################################################

cd /
rm -fr ${TMP1} ${TMP2} ${TMP3} ${TMP4}
