if ! `which pwd` -P; then
    exit 125;
fi

${PROOT} pwd -P
