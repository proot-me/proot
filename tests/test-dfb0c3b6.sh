if [ -z `which sh` ] || [ -z `which readlink` ] || [ -z `which grep` ] || [ -z `which echo` ] || [ ! -e /proc/self/fd/0 ]; then
    exit 125;
fi

${PROOT} / readlink /proc/self | grep -E "^[[:digit:]]+$"
! ${PROOT} / readlink /proc/self/..
${PROOT} / readlink /proc/self/../self | grep -E "^[[:digit:]]+$"

${PROOT} / sh -c 'echo "OK" | readlink /proc/self/fd/0' | grep -E "^pipe:\[[[:digit:]]+\]$"
! ${PROOT} / sh -c 'echo "OK" | readlink /proc/self/fd/0/'
! ${PROOT} / sh -c 'echo "OK" | readlink /proc/self/fd/0/..'
! ${PROOT} / sh -c 'echo "OK" | readlink /proc/self/fd/0/../0'

${PROOT} / sh -c 'echo "echo OK" | sh /proc/self/fd/0' | grep ^OK$

# XXX ${PROOT} / readlink /proc/self/exe | grep ^/bin/readlink$
# XXX ! ${PROOT} / readlink /proc/self/exe/
# XXX ! ${PROOT} / readlink /proc/self/exe/..
# XXX ! ${PROOT} / readlink /proc/self/exe/../exe
