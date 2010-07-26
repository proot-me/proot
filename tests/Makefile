NEW_ROOT=/tmp/proot/
PROOT=../proot

.PHONY: bug*

check: bug-0001 bug-0002

# Detranslate links pointing absolutely within the new root.
bug-0001:
	$(PROOT) $(NEW_ROOT) ls -l /proc/self/cwd | grep -- '-> /$$'

bug-0002:
	$(CC) $@.c -o $@
	$(PROOT) $(NEW_ROOT) $(PWD)/$@
