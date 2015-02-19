! ${PROOT} ${PROOT_RAW} /bin/true
if [ $? -eq 0 ]; then
    exit 125;
fi

echo exit 0 | ${PROOT} -v 0 ${PROOT_RAW} -v 0
