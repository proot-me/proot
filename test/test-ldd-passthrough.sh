if [ ! -e /bin/true ] || [ -z `which ldd` ]; then
    exit 125;
fi

${PROOT} ldd /bin/true
