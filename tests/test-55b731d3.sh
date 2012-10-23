if [ -z `which pwd` ]; then
    exit 125;
fi

${PROOT} pwd -P
