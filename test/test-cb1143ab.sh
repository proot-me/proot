if [ -z `which mcookie` ] || [ -z `which mkdir` ] ||  [ -z `which ln` ] ||  [ -z `which ls` ]; then
    exit 125;
fi

D1=`mcookie`
D2=`mcookie`
LINK=`mcookie`
F=`mcookie`
TMP=/tmp/${D1}/${D2}

mkdir -p ${TMP}
ln -s ${TMP}/./. ${TMP}/${LINK}

${PROOT} \ls ${TMP}/${LINK} | grep ^${LINK}$
${PROOT} \ls ${TMP}/${LINK}/ | grep ^${LINK}$
${PROOT} \ls ${TMP}/${LINK}/. | grep ^${LINK}$
${PROOT} \ls ${TMP}/${LINK}/.. | grep ^${D2}$
${PROOT} \ls ${TMP}/${LINK}/./.. | grep ^${D2}$

rm ${TMP}/${LINK}
touch ${TMP}/${F}
ln -s ${TMP}/${F} ${TMP}/${LINK}

${PROOT} \ls ${TMP}/${LINK}
! ${PROOT} \ls ${TMP}/${LINK}/
[ $? -eq 0 ]

! ${PROOT} \ls ${TMP}/${LINK}/.
[ $? -eq 0 ]

! ${PROOT} \ls ${TMP}/${LINK}/..
[ $? -eq 0 ]

! ${PROOT} \ls ${TMP}/${LINK}/./..
[ $? -eq 0 ]

! ${PROOT} \ls ${TMP}/${LINK}/../..
[ $? -eq 0 ]

${PROOT} -b /tmp/${D1}:${TMP}/${F} \ls ${TMP}/${LINK}
${PROOT} -b /tmp/${D1}:${TMP}/${F} \ls ${TMP}/${LINK}/
${PROOT} -b /tmp/${D1}:${TMP}/${F} \ls ${TMP}/${LINK}/.
${PROOT} -b /tmp/${D1}:${TMP}/${F} \ls ${TMP}/${LINK}/..

rm ${TMP}/${LINK}
ln -s ${TMP}/${D1} ${TMP}/${LINK}

${PROOT} -b /tmp/${F}:${TMP}/${D1} \ls ${TMP}/${LINK}
! ${PROOT} -b /tmp/${F}:${TMP}/${D1} \ls ${TMP}/${LINK}/
[ $? -eq 0 ]

! ${PROOT} -b /tmp/${F}:${TMP}/${D1} \ls ${TMP}/${LINK}/.
[ $? -eq 0 ]

! ${PROOT} -b /tmp/${F}:${TMP}/${D1} \ls ${TMP}/${LINK}/..
[ $? -eq 0 ]

rm -fr /tmp/${D1}
