#!/bin/bash

if [ -z ${PROOT_STAGE2} ]; then
    create_components ()
    {
	touch r${1}         2>/dev/null
	mkdir -p d${1}      2>/dev/null
	ln -fs r${1} rl${1}  2>/dev/null
	ln -fs d${1} dl${1}  2>/dev/null
    }

    create_components 1
    $(cd d1; create_components 2)

    env PROOT_STAGE2=1 ${PROOT} -w ${PWD} / sh -e ./$0
    exit $?
fi

set +e

TMP=/tmp/`mcookie`
mkdir -p /tmp

x1="r1 d1 rl1 dl1" # root of the test tree.
x2="r2 d2 rl2 dl2" # subtree of d1/dl1, every components exist.
x3="r3 d3 rl3 dl3" # subtree of d1/dl1, no component exists.
x4="/ /. /.."      # terminators.

make_tests ()
{
    for c in ${x4} ""; do
	x="${1}${c}"
	$(cd ${x}           2>/dev/null);      cd_result=$?
	/bin/cat ${x}       2>/dev/null;       cat_result=$?
	/bin/readlink ${x}  2>/dev/null 1>&2;  readlink_result=$?
	echo "${x}, $cd_result, $cat_result, $readlink_result" >> $TMP
    done
}

echo "path, chdir, cat, readlink" > $TMP
for a in ${x1}; do
    for b in ${x2}; do
	make_tests "${a}/${b}"
    done
    for b in ${x3}; do
	make_tests "${a}/${b}"
    done
done

set -e
cmp $TMP test-11111111.csv
rm $TMP
