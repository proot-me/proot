NEW_ROOT=/tmp/proot/
PROOT=../proot

.PHONY: bug*

check: bug-0001

# Detranslate links pointing absolutely within the new root.
bug-0001:
	$(PROOT) $(NEW_ROOT) ls -l /proc/self/cwd | grep -- '-> /$$'
