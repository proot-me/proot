if ! /usr/bin/pwd -P || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} / /usr/bin/pwd -P | grep '^/$'
