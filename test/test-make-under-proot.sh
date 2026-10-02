if [ -z `which make` ]; then
    exit 125;
fi

${PROOT} make -f ${PWD}/test-make-under-proot.mk
