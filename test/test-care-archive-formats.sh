if [ -z `which mkdir` ] || [ -z `which chmod` ] || [ -z `which touch` ] || [ -z `which ln` ] || [ -z `which cpio` ] || [ -z `which stat` ] || [ -z `which cat` ] || [ -z `which readlink` ] || [ -z `which mcookie` ] || [ -z `which mknod` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi
unset PROOT

TMP=/tmp/$(mcookie)
mkdir ${TMP}
cd ${TMP}

export LANG=en_US.UTF-8
mkdir a
echo "I'm a bee" > a/b
echo "I'm a sea" > a/c
chmod -w a
touch ł
ln ł d
ln -s dangling_symlink e
mknod f p
mkdir -p   x/y
chmod -rwx x

for BUNCH in \
    "FORMAT=/              EXTRACT=''" \
    "FORMAT=.raw           EXTRACT='${CARE} -x'" \
    "FORMAT=.cpio          EXTRACT='${CARE} -x'" \
    "FORMAT=.cpio.gz       EXTRACT='${CARE} -x'" \
    "FORMAT=.cpio.lzo      EXTRACT='${CARE} -x'" \
    "FORMAT=.tar           EXTRACT='${CARE} -x'" \
    "FORMAT=.tgz           EXTRACT='${CARE} -x'" \
    "FORMAT=.tar.gz        EXTRACT='${CARE} -x'" \
    "FORMAT=.tzo           EXTRACT='${CARE} -x'" \
    "FORMAT=.tar.lzo       EXTRACT='${CARE} -x'" \
    "FORMAT=.bin           EXTRACT='${CARE} -x'" \
    "FORMAT=.bin           EXTRACT='sh -c'" \
    "FORMAT=.gz.bin        EXTRACT='sh -c'" \
    "FORMAT=.lzo.bin       EXTRACT='sh -c'" \
    "FORMAT=.cpio.bin      EXTRACT='sh -c'" \
    "FORMAT=.cpio.gz.bin   EXTRACT='sh -c'" \
    "FORMAT=.cpio.lzo.bin  EXTRACT='sh -c'" \
    "FORMAT=.tar.bin       EXTRACT='sh -c'" \
    "FORMAT=.tgz.bin       EXTRACT='sh -c'" \
    "FORMAT=.tzo.bin       EXTRACT='sh -c'" \
    "FORMAT=.tar.gz.bin    EXTRACT='sh -c'" \
    "FORMAT=.tar.lzo.bin   EXTRACT='sh -c'"
do
    eval $BUNCH
    CWD=${PWD}

    if echo ${FORMAT} | grep '.bin' && ${CARE} -V | grep '(.bin): no'; then
	continue
    fi

    # Check: permissions, unordered archive, UTF-8, hard-links
    ${CARE} -o test${FORMAT} cat a/b ł d a/c

    if [ -n "${EXTRACT}" ]; then
	! chmod +rwx -R test-${FORMAT}-1
	rm -fr test-${FORMAT}-1
	mkdir test-${FORMAT}-1
	cd test-${FORMAT}-1
	${EXTRACT} ../test${FORMAT}
    fi

    test -d test/rootfs/${CWD}/a
    test -f test/rootfs/${CWD}/a/b
    test -f test/rootfs/${CWD}/a/c
    test -f test/rootfs/${CWD}/ł
    test -f test/rootfs/${CWD}/d

    INODE1=$(stat -c %i test/rootfs/${CWD}/d)
    INODE2=$(stat -c %i test/rootfs/${CWD}/ł)
    [ $INODE1 -eq $INODE2 ]

    PERM1=$(stat -c %a ${CWD}/a)
    PERM2=$(stat -c %a test/rootfs/${CWD}/a)
    [ $PERM1 -eq $PERM2 ]

    if [ -n "${EXTRACT}" ]; then
	cd ..
    else
	! chmod +rwx -R test
	rm -fr test
    fi

    # Check: last archived version wins, symlinks
    ${CARE} -o test${FORMAT} sh -c 'ls a; ls a/b; ls -l e'

    if [ -n "${EXTRACT}" ]; then
	! chmod +rwx -R test-${FORMAT}-2
	rm -fr test-${FORMAT}-2
	mkdir test-${FORMAT}-2
	cd test-${FORMAT}-2
	${EXTRACT} ../test${FORMAT}
    fi

    B=$(cat test/rootfs/${CWD}/a/b)
    [ x"$B" != x ]
    [ "$B" = "I'm a bee" ]

    test -L test/rootfs/${CWD}/e

    F=$(readlink test/rootfs/${CWD}/e)
    [ x"$F" != x ]
    [ "$F" = "dangling_symlink" ]

    if [ -n "${EXTRACT}" ]; then
	cd ..
    else
	! chmod +rwx -R test
	rm -fr test
    fi

    # Check: non-regular files are archived/extractable
    ${CARE} -d -p /dev -p /proc -o test${FORMAT} sh -c 'ls -l f'

    if [ -n "${EXTRACT}" ]; then
	! chmod +rwx -R test-${FORMAT}-1
	rm -fr test-${FORMAT}-1
	mkdir test-${FORMAT}-1
	cd test-${FORMAT}-1
	${EXTRACT} ../test${FORMAT}
    fi

    [ "fifo" = "$(stat -c %F test/rootfs/${CWD}/f)" ]

    if [ -n "${EXTRACT}" ]; then
	cd ..
    else
	! chmod +rwx -R test
	rm -fr test
    fi

    # Check: extractable archive
    ${CARE} -o test${FORMAT} chmod -R +rwx x

    if [ -n "${EXTRACT}" ]; then
	! chmod +rwx -R test-${FORMAT}-3
	rm -fr test-${FORMAT}-3
	mkdir test-${FORMAT}-3
	cd test-${FORMAT}-3
	${EXTRACT} ../test${FORMAT}

	cd ..
    else
	! chmod +rwx -R test
	rm -fr test
    fi
done

cd ..
chmod +rwx -R ${TMP}
rm -fr ${TMP}

