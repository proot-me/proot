if [ -z `which mknod`] || [ `id -u` -eq 0 ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)

! ${PROOT} mknod ${TMP} b 1 1
[ $? -eq 0 ]

${PROOT} -0 mknod ${TMP} b 1 1
