if [ -z `which sh` ] || [ -z `which readlink` ] || [ -z `which grep` ] || [ -z `which echo` ]  || [ -z `which mcookie` ] || [ ! -e /proc/self/fd/0 ]; then
    exit 125;
fi

${PROOT} readlink /proc/self | grep -E "^[[:digit:]]+$"

! ${PROOT} readlink /proc/self/..
[ $? -eq 0 ]

${PROOT} readlink /proc/self/../self | grep -E "^[[:digit:]]+$"

${PROOT} sh -c 'echo "OK" | readlink /proc/self/fd/0' | grep -E "^pipe:\[[[:digit:]]+\]$"

! ${PROOT} sh -c 'echo "OK" | readlink /proc/self/fd/0/'
[ $? -eq 0 ]

! ${PROOT} sh -c 'echo "OK" | readlink /proc/self/fd/0/..'
[ $? -eq 0 ]

! ${PROOT} sh -c 'echo "OK" | readlink /proc/self/fd/0/../0'
[ $? -eq 0 ]

${PROOT} sh -c 'echo "echo OK" | sh /proc/self/fd/0' | grep ^OK$

TMP=/tmp/$(mcookie)
${PROOT} sh -c "exec 6<>${TMP}; readlink /proc/self/fd/6" | grep ^${TMP}
rm -f ${TMP}
