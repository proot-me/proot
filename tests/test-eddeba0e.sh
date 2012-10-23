if [ -z `which pwd` ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} pwd -P | grep "^$PWD$"
