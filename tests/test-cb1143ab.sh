if [ -z `which mcookie` ] || [ -z `which mkdir` ] ||  [ -z `which ln` ] ||  [ -z `which ls` ]; then
    exit 125;
fi

D1=`mcookie`
D2=`mcookie`
TMP=/tmp/${D1}/${D2}

mkdir -p ${TMP}
ln -s ${TMP}/./. ${TMP}/link

${PROOT} / \ls ${TMP}/link | grep ^link$
${PROOT} / \ls ${TMP}/link/ | grep ^link$
${PROOT} / \ls ${TMP}/link/. | grep ^link$
${PROOT} / \ls ${TMP}/link/.. | grep ^${D2}$
${PROOT} / \ls ${TMP}/link/./.. | grep ^${D2}$

rm -fr /tmp/${D1}
