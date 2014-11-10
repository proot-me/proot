if [ -z `which mkdir` ] || [ -z `which rm` ] || [ -z `which mcookie` ] || [ -z `which chmod` ] || [ -z `which ln` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi
unset PROOT

SYMLINK=/tmp/$(mcookie)
FOLDER=/tmp/$(mcookie)
SCRIPT=${FOLDER}/script.sh
ARCHIVE=/tmp/$(mcookie)/

mkdir ${FOLDER}

echo "true" > ${SCRIPT}
chmod +x ${SCRIPT}

ln -s ${FOLDER} ${SYMLINK}

cd ${SYMLINK}
${CARE} -r ${FOLDER} -o ${ARCHIVE} sh ./script.sh

test -e ${ARCHIVE}/rootfs${SCRIPT}

${ARCHIVE}/re-execute.sh

rm -fr ${SYMLINK} ${FOLDER} ${ARCHIVE}
