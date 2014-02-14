if [ -z `which mcookie` ] || [ -z `which id` ] || [ -z `which mkdir` ] || [ -z `which touch` ] || [ -z `which chmod` ] || [ -z `which stat` ] || [ -z `which grep` ]; then
    exit 125;
fi

if [ `id -u` -eq 0 ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)

mkdir -p ${TMP}/foo
chmod a-rwx ${TMP}/foo
chmod a-rwx ${TMP}

! ${PROOT} touch ${TMP}/foo/bar
[ $? -eq 0 ]

! ${PROOT} -i 123:456 touch ${TMP}/foo/bar
[ $? -eq 0 ]

${PROOT} -0 touch ${TMP}/foo/bar

stat -c %a ${TMP} | grep '^0$'
! stat -c %a ${TMP}/foo
[ $? -eq 0 ]

! ${PROOT} -i 123:456 stat -c %a ${TMP}/foo | grep '^0$'
[ $? -eq 0 ]

${PROOT} -0 stat -c %a ${TMP}/foo | grep '^0$'

chmod -R a+rwx ${TMP}
chmod a-rwx ${TMP}/foo/bar
chmod a-rwx ${TMP}/foo
chmod a-rwx ${TMP}

! ${PROOT} chmod g+w ${TMP}/foo/bar
[ $? -eq 0 ]

! ${PROOT} -i 123:456 chmod g+w ${TMP}/foo/bar
[ $? -eq 0 ]

${PROOT} -0 chmod g+w ${TMP}/foo/bar

chmod u+wx ${TMP}
chmod u+x ${TMP}/foo

stat -c %a ${TMP}/foo/bar | grep '^20$'

chmod -R +rwx ${TMP}
rm -fr ${TMP}

mkdir -p ${TMP}/foo
chmod -rwx ${TMP}

! rm -fr ${TMP}
[ $? -eq 0 ]

! ${PROOT} -i 123:456 rm -fr ${TMP}
[ $? -eq 0 ]

${PROOT} -0 rm -fr ${TMP}
