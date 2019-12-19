if [ -z `which mcookie` ] || [ -z `which cat` ] || [ -z `which grep` ] || [ -z `which chmod` ] || [ -z `which cut` ] || [ -z `which rm` ] || [ -z `which ln` ] || [ -z `which env` ]; then
    exit 125;
fi

TMP=$(mcookie)
TMP2=$(echo ${TMP} | cut -b 1-15)
TMP3=$(mcookie)
TMP4=$(echo ${TMP3} | cut -b 1-15)

${PROOT} cat /proc/self/comm          | grep cat
${PROOT} $(which cat) /proc/self/comm | grep cat

echo '#!/bin/sh' > /tmp/${TMP}
chmod +x /tmp/${TMP}

# TODO: (cd /tmp; ${PROOT} env LD_SHOW_AUXV=1 ./${TMP}) | grep ^AT_EXECFN:[[:space:]]*./${TMP}$

echo 'cat /proc/$$/comm' >> /tmp/${TMP}

${PROOT} /tmp/${TMP} | grep ^${TMP2}$

ln -s /tmp/${TMP} /tmp/${TMP3}
${PROOT} /tmp/${TMP3} /proc/self/comm | grep ^${TMP4}$

rm -f /tmp/${TMP} /tmp/${TMP3}
