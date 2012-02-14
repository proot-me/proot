SHELL=/bin/bash
FOO:=$(shell test -e /dev/null && echo OK)

all:
	@/usr/bin/test -n "$(FOO)"
