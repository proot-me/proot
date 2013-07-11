if [ -z `which mcookie` ] || [ -z `which cp` ] || [ -z `which true` ] || [ -z `which setcap` ] || [ -z `which rm` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)

cp $(which true) ${TMP}
${PROOT} -0 setcap cap_setuid+ep ${TMP}

rm -f ${TMP}
