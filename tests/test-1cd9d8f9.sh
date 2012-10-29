if ! `which pwd` -P || [ -z `which grep` ] ; then
    exit 125;
fi

${PROOT} -w /tmp pwd -P | grep '^/tmp$'
