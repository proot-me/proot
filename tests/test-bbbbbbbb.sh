if [ -z `which mcookie` ] || [ -z `which rm` ] || [ -z `which ln` ]; then
    exit 125;
fi

DONT_EXIST=$(mcookie)

rm -f /tmp/${DONT_EXIST}
${PROOT} ln -sf /${DONT_EXIST} /tmp/
${PROOT} ln -sf /${DONT_EXIST} /tmp/

rm -f /tmp/${DONT_EXIST}
  ${PROOT} ln -sf /etc/fstab/${DONT_EXIST} /tmp/
! ${PROOT} ln -sf /etc/fstab/${DONT_EXIST} /tmp/

rm -f /tmp/${DONT_EXIST}
