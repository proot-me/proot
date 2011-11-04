# I will write a better Makefile soon...

ROOTFS=./rootfs
PROOT=../src/proot

check: test-0001 test-0002

test-0001:
	$(PROOT) -w /tmp / /bin/sh -c 'echo $$PWD' | grep '^/tmp$$'

test-0002:
	rm -fr $(ROOTFS)/test-0002
	$(PROOT) -b /etc/fstab:/test-0002/fstab -b /tmp:/test-0002/tmp $(ROOTFS) /bin/false 2>/dev/null || true
	cd  $(ROOTFS)/test-0002/fstab && false
	cat $(ROOTFS)/test-0002/fstab
	cd  $(ROOTFS)/test-0002/tmp
	cat $(ROOTFS)/test-0002/tmp && false

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

