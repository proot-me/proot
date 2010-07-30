# I will write a better Makefile soon...

NEW_ROOT=/tmp/proot/
PROOT=../proot

.PHONY: bug*

check: bug-0001 bug-0002 bug-0003 bug-0004 bug-0005 bug-0006 bug-0007

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

