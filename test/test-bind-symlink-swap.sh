if [ -z `which mcookie` ] ||  [ -z `which echo` ] || [ -z `which touch` ] || [ -z `which rm` ]; then
    exit 125;
fi

FOO1=/tmp/$(mcookie)
FOO2=/tmp/$(mcookie)
FOO3=/tmp/$(mcookie)
FOO4=/tmp/$(mcookie)

echo "content of FOO1" > ${FOO1}
echo "content of FOO2" > ${FOO2}

ln -s ${FOO1} ${FOO3} # FOO3 -> FOO1
ln -s ${FOO2} ${FOO4} # FOO4 -> FOO2

${PROOT} -b ${FOO3}:${FOO4} cat ${FOO2} | grep '^content of FOO1$'
${PROOT} -b ${FOO4}:${FOO3} cat ${FOO1} | grep '^content of FOO2$'

${PROOT} -b ${FOO3}:${FOO4}! cat ${FOO2} | grep '^content of FOO2$'
${PROOT} -b ${FOO4}:${FOO3}! cat ${FOO1} | grep '^content of FOO1$'

${PROOT} -v -1 -b ${FOO1} -b ${FOO3}                 cat ${FOO1} | grep '^content of FOO1$'
${PROOT} -v -1 -b ${FOO1} -b ${FOO2}:/tmp/../${FOO1} cat ${FOO1} | grep '^content of FOO2$'

rm -f ${FOO1} ${FOO2} ${FOO3}
