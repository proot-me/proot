if [ -z `which make` ]; then
    exit 125;
fi

${PROOT} make -f ${PWD}/test-c6b77b77.mk
