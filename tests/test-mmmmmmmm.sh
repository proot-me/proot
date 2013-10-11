if [ -z `which mcookie` ] || [ -z `which rmdir` ] || [ -z `which mkdir` ]; then
    exit 125;
fi

TMP=$(mcookie)
cd /tmp

${PROOT} mkdir ./${TMP}
${PROOT} rmdir ./${TMP}

${PROOT} mkdir ${TMP}/
${PROOT} rmdir ${TMP}/

${PROOT} mkdir ./${TMP}/
${PROOT} rmdir ./${TMP}/
