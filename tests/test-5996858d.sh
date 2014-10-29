if [ -z `which uname` ] || [ -z `which grep` ] || [ -z `which domainname` ] || [ -z `which env` ] || [ -z `which true`]; then
    exit 125;
fi

UTSNAME="\\sysname\\nodename\\$(uname -r)\\version\\machine\\domainname\\0\\"

${PROOT} -k ${UTSNAME} uname -s | grep ^sysname$
${PROOT} -k ${UTSNAME} uname -n | grep ^nodename$
${PROOT} -k ${UTSNAME} uname -v | grep ^version$
${PROOT} -k ${UTSNAME} uname -m | grep ^machine$
${PROOT} -k ${UTSNAME} domainname | grep ^domainname$
${PROOT} -k ${UTSNAME} env LD_SHOW_AUXV=1 true | grep '^AT_HWCAP:[[:space:]]*0$'
