if [ -z `which uname` ] || [ -z `which grep` ] || [ -z `which domainname` ] || [ -z `which hostname` ]|| [ -z `which env` ] || [ -z `which true`]; then
    exit 125;
fi

UTSNAME="\\sysname\\nodename\\$(uname -r)\\version\\machine\\domainname\\0\\"

${PROOT} -k ${UTSNAME} uname -s | grep ^sysname$
${PROOT} -k ${UTSNAME} uname -n | grep ^nodename$
${PROOT} -k ${UTSNAME} uname -v | grep ^version$
${PROOT} -k ${UTSNAME} uname -m | grep ^machine$
${PROOT} -k ${UTSNAME} domainname | grep ^domainname$
${PROOT} -k ${UTSNAME} env LD_SHOW_AUXV=1 true | grep -E '^AT_HWCAP:[[:space:]]*0?$'

${PROOT} -0 -k ${UTSNAME} sh -c 'domainname domainname2; domainname' | grep ^domainname2$
${PROOT} -0 -k ${UTSNAME} sh -c 'hostname hostname2; hostname' | grep ^hostname2$
${PROOT} -0 -k ${UTSNAME} sh -c 'hostname hostname2; uname -n' | grep ^hostname2$
