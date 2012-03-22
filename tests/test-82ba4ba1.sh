if [ -z 'which grep' ]; then
    exit 125;
fi

${PROOT} -0 / id -u | grep ^0$
${PROOT} -0 / id -g | grep ^0$
${PROOT} -0 / chown root.root /root
