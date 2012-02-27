if ! /usr/bin/pwd -P || [ -z 'which grep' ] ; then
    exit 125;
fi

${PROOT} -w /tmp / /usr/bin/pwd -P | grep '^/tmp$'
