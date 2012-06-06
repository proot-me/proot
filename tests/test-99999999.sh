if [ -z `which grep` ] || [ -z `which md5sum` ]; then
    exit 125;
fi

MD5=$(md5sum $(which md5sum) | cut -f 1 -d ' ')

MD5_PROOT=$(${PROOT} / md5sum /proc/self/exe | cut -f 1 -d ' ')
test ${MD5_PROOT} = ${MD5}

MD5_PROOT=$(${PROOT} -b /proc / md5sum /proc/self/exe | cut -f 1 -d ' ')
test ${MD5_PROOT} = ${MD5}
