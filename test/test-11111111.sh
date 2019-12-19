#!/bin/bash

if [ -z `which cat` ] || [ -z `which readlink` ] || [ -z `which mcookie` ] || [ -z `which touch` ] || [ -z `which mkdir` ] || [ -z `which ln` ] || [ -z `which grep` ] || [ -z `which rm` ]; then
    exit 125;
fi

set +e

x1="r1 d1 rl1 dl1" # root of the test tree.
x2="r2 d2 rl2 dl2" # subtree of d1/dl1, every components exist.
x3="r3 d3 rl3 dl3" # subtree of d1/dl1, no component exists.
x4="/ /. /.."      # terminators.

generate () {
    output=${1}

    make_tests ()
    {
	for c in ${x4} ""; do
	    x="${1}${c}"
	    $(cd ${x}      2>/dev/null);      cd_result=$?
	    cat ${x}       2>/dev/null;       cat_result=$?
	    readlink ${x}  2>/dev/null 1>&2;  readlink_result=$?
	    echo "${x}, $cd_result, $cat_result, $readlink_result" >> $output
	done
    }

    echo "path, chdir, cat, readlink" > $output
    for a in ${x1}; do
	for b in ${x2}; do
	    make_tests "${a}/${b}"
	done
	for b in ${x3}; do
	    make_tests "${a}/${b}"
	done
    done
}

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

    REF=/tmp/`mcookie`
    mkdir -p /tmp

    generate $REF

    env PROOT_STAGE2=$REF ${PROOT} -w ${PWD} sh ./$0
    exit $?
fi

TMP=/tmp/`mcookie`
mkdir -p /tmp

generate $TMP

set -e
cmp $TMP $PROOT_STAGE2
rm $TMP $PROOT_STAGE2
