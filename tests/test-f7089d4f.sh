if [ -z `which timeout` ] || [ -z `which msgmerge` ] || [ ! -e /dev/null ]; then
    exit 125;
fi

timeout 5s ${PROOT} msgmerge -q /dev/null /dev/null
