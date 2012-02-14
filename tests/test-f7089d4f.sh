if [ -z `which timeout` ] || [ ! -x /usr/bin/msgmerge ]; then
    exit 125;
fi

timeout 5s ${PROOT} / /usr/bin/msgmerge -q /dev/null /dev/null
