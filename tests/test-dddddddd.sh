if [ -z `which mcookie` ] || [ -z `which rm` ] || [ -z `which ln` ] || [ -z `which rmdir` ] || [ -z `which env` ]; then
    exit 125;
fi

TMP="/tmp/$(mcookie)"
TMP2="/tmp/$(mcookie)"

export LAND=C

ln -s /bin ${TMP}
! rmdir ${TMP} > ${TMP}.ref 2>&1
! ${PROOT} -v -1 rmdir ${TMP} > ${TMP}.res 2>&1
cmp ${TMP}.ref ${TMP}.res

ln -s /this/does/not/exist ${TMP2}
! mkdir ${TMP2} > ${TMP2}.ref 2>&1
! ${PROOT} -v -1 mkdir ${TMP2} > ${TMP2}.res 2>&1
cmp ${TMP2}.ref ${TMP2}.res

rm -f ${TMP}
rm -f ${TMP}.ref
rm -f ${TMP}.res

rm -f ${TMP2}
rm -f ${TMP2}.ref
rm -f ${TMP2}.res
