if [ ! -x ${ROOTFS}/bin/readdir ] || [ ! -e /bin/true ] || [ -z `which mkdir` ] || [ -z `which ln` ]  || [ -z `which rm` ] || [ -z `which grep` ] || [ -z `which mcookie` ]; then
    exit 125;
fi

TMP=/tmp/$(mcookie)

mkdir -p ${ROOTFS}/${TMP}/run/dbus
mkdir -p ${ROOTFS}/${TMP}/var
ln -s ../run ${ROOTFS}/${TMP}/var/run

${PROOT} -b /bin:${TMP}/var/run/dbus -r ${ROOTFS} readdir ${TMP}/var/run/dbus/ | grep true

rm -fr ${TMP}
