if ! /bin/pwd -P > /dev/null 2>&1; then
    exit 125;
fi

${PROOT} -w /tmp / /bin/pwd -P | grep '^/tmp$'
