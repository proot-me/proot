if [ -z `which mknod` || `id -u` = 0 ]; then
    exit 125;
fi

! ${PROOT} mknod /tmp/test b 1 1
[ $? -eq 0 ]

${PROOT} -0 mknod /tmp/test b 1 1
