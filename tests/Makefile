# I will write a better Makefile soon...

NEW_ROOT=/tmp/proot/
PROOT=../src/proot

.PHONY: bug*

check: test-0001

test-0001:
	$(PROOT) -w /tmp / /bin/sh -c 'echo $$PWD' | grep '^/tmp$$'

# Detranslate links pointing absolutely within the new root.
bug-0001:
	$(PROOT) $(NEW_ROOT) ls -l /proc/self/cwd | grep -- '-> /$$'

bug-0002:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@

bug-0003:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@

bug-0004:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@

bug-0005:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@

bug-0006:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@ | egrep "child: / == parent: /"

bug-0007:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@

bug-0008:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@

