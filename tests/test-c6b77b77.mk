SHELL=/lib64/ld-linux-x86-64.so.2 /bin/sh
FOO:=$(shell test -e /dev/null && echo OK)

all:
	@/usr/bin/test -n "$(FOO)"
