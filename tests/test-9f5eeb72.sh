if [ -z `which mkdir` ] || [ -z `which chmod` ] || [ -z `which touch` ] || [ -z `which ln` ] || [ -z `which cpio` ] || [ -z `which stat` ] || [ -z `which cat` ] || [ -z `which readlink` ] || [ -z `which mcookie` ]; then
    exit 125;
fi

if [ ! -e $CARE ]; then
    exit 125;
fi

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

for BUNCH in "FORMAT=cpio EXTRACT='cpio -idmuvF'"
# "FORMAT=tar  EXTRACT='tar -xf'"
do
    eval $BUNCH
    CWD=${PWD}

    # Check: permissions, unordered archive, UTF-8, hard-links
    ${CARE} -o test.${FORMAT} cat a/b ł d a/c

    mkdir test-${FORMAT}-1
    cd test-${FORMAT}-1
    ${EXTRACT} ../test.${FORMAT}

    test -d test/rootfs/${CWD}/a
    test -f test/rootfs/${CWD}/a/b
    test -f test/rootfs/${CWD}/a/c
    test -f test/rootfs/${CWD}/ł
    test -f test/rootfs/${CWD}/d

    INODE1=$(stat -c %i test/rootfs/${CWD}/d)
    INODE2=$(stat -c %i test/rootfs/${CWD}/ł)
    [ $INODE1 -eq $INODE2 ]

    PERM1=$(stat -c %a ../a)
    PERM2=$(stat -c %a test/rootfs/${CWD}/a)
    [ $PERM1 -eq $PERM2 ]

    cd ..

    # Check: last archived version wins, symlinks
    ${CARE} -o test.${FORMAT} sh -c 'ls a; ls a/b; ls -l e'

    mkdir test-${FORMAT}-2
    cd test-${FORMAT}-2
    ${EXTRACT} ../test.${FORMAT}

    B=$(cat test/rootfs/${CWD}/a/b)
    [ x"$B" != x ]
    [ "$B" = "I'm a bee" ]

    test -L test/rootfs/${CWD}/e

    F=$(readlink test/rootfs/${CWD}/e)
    [ x"$F" != x ]
    [ "$F" = "dangling_symlink" ]

    cd ..
done

cd ..
chmod +w -R ${TMP}
rm -fr ${TMP}

