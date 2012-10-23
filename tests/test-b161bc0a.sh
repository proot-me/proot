if [ -z `which pwd` ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} -w /tmp/a -m /etc:/tmp/a pwd | grep '^/tmp/a$'

