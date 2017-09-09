if [ ! -x /bin/sh ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} -w /tmp /bin/sh -c 'echo $PWD' | grep '^/tmp$'
