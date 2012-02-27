if ! /usr/bin/pwd || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} -w /tmp/a -m /etc:/tmp/a / /usr/bin/pwd | grep '^/tmp/a$'

