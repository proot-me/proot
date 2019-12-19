if [ -z `which sh` ] || [ -z `which kill` ] || [ -z `which grep` ] || [ -z `which cut` ]; then
    exit 125;
fi

${PROOT} sh -c 'kill -15 $(grep TracerPid /proc/self/status | cut -f 2 -d :)'
