if [ -z `which timeout` ] || [ -z `which msgmerge` ]; then
    exit 0;
fi

timeout 5s ${PROOT} / /usr/bin/msgmerge -q /dev/null /dev/null
