if [ -z `which env` ] || [ -z `which true` ]; then
    exit 125;
fi

env PROOT_NO_SUBRECONF=1 ${PROOT} ${PROOT} -v 1 true
