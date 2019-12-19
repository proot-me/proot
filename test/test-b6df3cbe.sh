if [ -z `which mcookie` ] ||  [ -z `which cat` ] || [ -z `which tr` ] || [ -z `which grep` ] || [ -z `which grep` ] || [ -z `which chmod` ]; then
    exit 125;
fi

TMP=$(mcookie)

cat > /tmp/${TMP} <<EOF
#!/bin/sh

cat /proc/\$$/cmdline
EOF

chmod +x /tmp/${TMP}
(cd /tmp; ${PROOT} sh -c "./${TMP}") | tr '\000' ' ' | grep "^/bin/sh ./${TMP} $"

rm /tmp/${TMP}
