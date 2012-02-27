if ! /usr/bin/pwd -P; then
    exit 125;
fi

${PROOT} / /usr/bin/pwd -P
