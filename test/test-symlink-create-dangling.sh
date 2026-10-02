if [ -z `which mcookie` ] || [ -z `which rm` ] || [ -z `which ln` ]; then
    exit 125;
fi

DONT_EXIST=$(mcookie)
TMP1=$(mcookie)
TMP2=$(mcookie)

rm -f /tmp/${DONT_EXIST}
${PROOT} ln -sf /${DONT_EXIST} /tmp/
${PROOT} ln -sf /${DONT_EXIST} /tmp/

rm -f /tmp/${DONT_EXIST}
  ${PROOT} ln -sf /etc/fstab/${DONT_EXIST} /tmp/
! ${PROOT} ln -sf /etc/fstab/${DONT_EXIST} /tmp/

rm -f /tmp/${DONT_EXIST}
rm -f /tmp/${TMP1} /tmp/${TMP2}
touch /tmp/${TMP2}
ln -sf /tmp/${DONT_EXIST} /tmp/${TMP1}
! ${PROOT} ln /tmp/${TMP2} /tmp/${TMP1}

rm -f /tmp/${TMP1} /tmp/${TMP2}
rm -f /tmp/${DONT_EXIST}
