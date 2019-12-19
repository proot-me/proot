if [ -z `which id` ] || [ -z `which uname` ] || [ -z `which grep` ]; then
    exit 125;
fi

${PROOT} -v -1 -i $(id -u):$(id -g) -0 id -u | grep "^0$"
${PROOT} -v -1 -0 -i $(id -u):$(id -g) id -u | grep "^$(id -u)$"
${PROOT} -v -1 -0 -i $(id -u):$(id -g) -0 id -u | grep "^0$"

${PROOT} -v -1 -k $(uname -r) -k 5.0 uname -r | grep "^5.0$"
${PROOT} -v -1 -k 5.0 -k $(uname -r) uname -r | grep "^$(uname -r)$"
${PROOT} -v -1 -k 5.0 -k $(uname -r) -k 5.0 uname -r | grep "^5.0$"
