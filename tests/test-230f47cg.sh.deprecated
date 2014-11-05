if [ ! -x  ${ROOTFS}/bin/cat ] || [ -z `which mcookie` ] || [ -z `which echo` ] || [ -z `which cp` ] || [ -z `which grep` ]|| [ -z `which rm` ]; then
    exit 125;
fi

! ${PROOT} ${PROOT_RAW} /bin/true
if [ $? -eq 0 ]; then
    exit 125;
fi

FOO1=/tmp/$(mcookie)
FOO2=/tmp/$(mcookie)
ROOTFS2=/$(mcookie)
FOO3=/tmp/$(mcookie)

mkdir -p ${ROOTFS}/tmp
mkdir -p ${ROOTFS}/${ROOTFS2}/bin
cp ${ROOTFS}/bin/cat ${ROOTFS}/${ROOTFS2}/bin/cat

echo "content of foo1" > ${FOO1}
echo "content of foo2" > ${FOO2}
echo "content of foo3" > ${ROOTFS}/${FOO3}

CMD="${PROOT}	-r ${ROOTFS}				\
		-b ${FOO2}				\
		-b ${FOO1}:${ROOTFS2}/${FOO1}		\
		-b ${FOO2}:${ROOTFS2}/${FOO2}		\
		-b ${PROOT_RAW}				\
		${PROOT_RAW}	-r ${ROOTFS2}		\
				-b /:/host-rootfs	\
				-b ${FOO3}:${FOO2}	\
				-v -1"

${CMD} cat /${FOO1}		 | grep '^content of foo1$'
${CMD} cat /host-rootfs/${FOO2}	 | grep '^content of foo2$'
${CMD} cat /${FOO2}		 | grep '^content of foo3$'

rm -fr ${FOO1}
rm -fr ${FOO2}
rm -fr ${ROOTFS2}
rm -fr ${ROOTFS}/${FOO3}
