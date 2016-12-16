#!/bin/sh

[ -e slackwarearm-14.1 ] || {
	echo "Run script init-build-env-arm.sh first"
	exit 1
}

PATH=`pwd`:$PATH
which proot > /dev/null || {
	echo "Please compile proot first, and copy it to the current directory"
	exit 1
}

# Build PRoot/ARM statically:
SRC=`cd ../src ; pwd`
[ -e talloc-arm ] || {
	[ -e talloc.tar.gz ] || wget http://www.samba.org/ftp/talloc/talloc-2.1.3.tar.gz -O talloc.tar.gz || exit 1
	rm -rf talloc-arm
	tar xvf talloc.tar.gz || exit 1
	mv talloc-2.1.3 talloc-arm || exit 1
	cd talloc-arm || exit 1
	rm -f libtalloc.a
	proot -R ../slackwarearm-14.1 -q qemu-arm ./configure build || exit 1
	ar rcs libtalloc.a bin/default/talloc*.o || exit 1
	cd ..
}

make -C $SRC clean
proot -R slackwarearm-14.1 -q qemu-arm env CPPFLAGS="-I`pwd`/talloc-arm" LDFLAGS="-L`pwd`/talloc-arm -static" make -C $SRC glibc-version=glibc-2.18 -f GNUmakefile -j4 || exit 1
mv -f $SRC/proot $SRC/proot-arm || exit 1
