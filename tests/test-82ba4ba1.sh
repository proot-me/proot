if [ -z `which id` ] || [ -z `which grep` ] || [ -z `which chown` ] || [ -z `which chroot` ]  || [ ! /bin/true ] ; then
    exit 125;
fi

${PROOT} -0 / id -u | grep ^0$
${PROOT} -0 / id -g | grep ^0$
${PROOT} -0 / chown root.root /root
${PROOT} -0 / chroot / /bin/true
${PROOT} -0 / chroot /tmp/.. /bin/true
