DIR = $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
ROOTFS = $(DIR)/rootfs
SRC_DIR = $(DIR)/../src
PROOT = $(SRC_DIR)/proot
CARE = $(SRC_DIR)/care
CC = gcc
PROOT_RAW = $(PROOT)

CHECK_TESTS = $(patsubst %,check-%, $(wildcard test-*.sh) $(wildcard test-*.c))

.PHONY: check clean_failure check_failure setup check-%

check: | clean_failure check_failure

memcheck: PROOT_RAW := $(PROOT)
memcheck: PROOT := $(shell which valgrind) -q --error-exitcode=1 $(PROOT)
memcheck: check

clean_failure:
	@rm -f failure

check_failure: $(CHECK_TESTS)
	@bash -c '! test -e failure'

check-%.sh: %.sh setup
	$(Q)env CARE="$(CARE)" PROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) PATH="$${PATH}:/bin" sh -ex $< $(silently); $(call check,$*)

check-%.c: $(ROOTFS)/bin/% setup
	$(call check_c,$*,$(PROOT) -b /proc -r $(ROOTFS) /bin/$*)

C_SHORT_TESTS = $(patsubst %.c,check-%,$(wildcard test-*.c))
SH_SHORT_TESTS = $(patsubst %.sh,check-%,$(wildcard test-*.sh))
.PHONY: $(C_SHORT_TESTS) $(SH_SHORT_TESTS)
$(C_SHORT_TESTS): check-%: check-%.c
$(SH_SHORT_TESTS): check-%: check-%.sh



# Special cases.
check-test-sysexit.c: test-sysexit
	$(call check_c,$<,env PROOT_FORCE_KOMPAT=1 $(PROOT) -k 3.4242XX ./$<)

check-test-rlimit-stack-brk.c: test-rlimit-stack-brk
	$(call check_c,$<,$(PROOT) -w . ./$<)

check-test-chdir-thread-fork.c: test-chdir-thread-fork
	$(call check_c,$<,$(PROOT) ./$<)

check-test-cwd-rename-rmdir.c: test-cwd-rename-rmdir
	$(call check_c,$<,$(PROOT) -r $(ROOTFS) -b . ./$<)
	$(call check_c,$<,$(PROOT) ./$<)

check-test-fork-vfork-execve.c: test-fork-vfork-execve
	$(call check_c,$<,$(PROOT) ./$<)
	$(call check_c,$<,$(PROOT) ./$< 1)

check-test-getresuid-getresgid-syscalls.c: test-getresuid-getresgid-syscalls
	$(call check_c,$<,$(PROOT) -0 ./$<)
	$(call check_c,$<,! $(PROOT) -i 123:456 ./$<)

check-test-brk-syscall.c: test-brk-syscall
	$(call check_c,$<,$(PROOT) ./$<)

check-test-exec-proc-self-exe.c: test-exec-proc-self-exe
	$(call check_c,$<,$(PROOT) ./$<)

check-test-exec-proc-self-exe-null-argv.c: test-exec-proc-self-exe-null-argv
	$(call check_c,$<,$(PROOT) ./$<)

check-test-pipe2-nonblock.c: test-pipe2-nonblock
	$(call check_c,$<,env PROOT_FORCE_KOMPAT=1 $(PROOT) -k $(shell uname -r) ./$<)

check-test-setuid-thread-vs-process.c: test-setuid-thread-vs-process
	$(call check_c,$<,$(PROOT) -0 ./$<)
	$(call check_c,$<,! $(PROOT) -i 123:456 ./$<)

#check-test-a3e68988.c: test-a3e68988
#	@which gdb >/dev/null 2>&1 || rm -f $<
#	$(call check_c,$<,$(PROOT) gdb -return-child-result -ex run -ex quit ./$<)

#check-test-c47aeb7d.c: test-c47aeb7d
#	@which gdb >/dev/null 2>&1 || rm -f $<
#	$(call check_c,$<,$(PROOT) gdb -return-child-result -ex run -ex quit ./$<)

check-test-openat-fd-edge-cases.c: test-openat-fd-edge-cases
	$(call check_c,$<,echo test | $(PROOT) ./$<)

check-test-faccessat-nofollow-ignored.c: test-faccessat-nofollow-ignored
	$(call check_c,$<,echo test | env PROOT_DONT_POLLUTE_ROOTFS=1 $(PROOT) -b /bin:/this_shall_not_exist_outside_proot ./$<)

check-test-constructor-runs-once.c: test-constructor-runs-once
	$(call check_c,$<,$(PROOT) ./$<)

check-test-thread-execve-nyi.c: test-thread-execve-nyi
	$(call check_c,$<,$(PROOT) ./$<)

check-test-bug-182.c: test-bug-182
	$(call check_c,$<,$(PROOT) ./$<)

check_c = $(Q)if [ -e $< ]; then			\
		$(2) $(silently); $(call check,$(1))	\
	else						\
		echo "  CHECK	$(1) skipped";		\
	fi

check = case "$$?" in					\
		0)   echo "  CHECK	$(1) ok";;	\
		125) echo "  CHECK	$(1) skipped";;	\
		*)   echo "  CHECK	$(1) FAILED";	\
		     touch failure ;;			\
	esac

######################################################################
# Build a clean rootfs

ROOTFS_BIN = $(ROOTFS)/bin/true $(ROOTFS)/bin/false    				 \
       $(ROOTFS)/bin/pwd  $(ROOTFS)/bin/readlink $(ROOTFS)/bin/symlink		 \
       $(ROOTFS)/bin/abs-true $(ROOTFS)/bin/rel-true $(ROOTFS)/bin/echo		 \
       $(ROOTFS)/bin/argv0 $(ROOTFS)/bin/readdir $(ROOTFS)/bin/cat		 \
       $(ROOTFS)/bin/chdir_getcwd $(ROOTFS)/bin/fchdir_getcwd $(ROOTFS)/bin/argv \
       $(ROOTFS)/bin/fork-wait $(ROOTFS)/bin/ptrace $(ROOTFS)/bin/ptrace-2	 \
       $(ROOTFS)/bin/ptrace-3 $(ROOTFS)/bin/gdb-ptrace-test $(ROOTFS)/bin/gdb-ptrace-test-signal	\
       $(ROOTFS)/bin/puts_proc_self_exe $(ROOTFS)/bin/exec $(ROOTFS)/bin/exec-m32 \
       $(ROOTFS)/bin/exec-suid $(ROOTFS)/bin/exec-sgid $(ROOTFS)/bin/exec-m32-suid \
       $(ROOTFS)/bin/exec-m32-sgid $(ROOTFS)/bin/getresuid $(ROOTFS)/bin/getresgid \
       $(ROOTFS)/bin/chroot $(ROOTFS)/bin/execfn $(ROOTFS)/bin/execfn-m32 \
       $(ROOTFS)/bin/readlink-fd $(ROOTFS)/bin/no-new-privs

ROOTFS_DIR = $(ROOTFS)/bin $(ROOTFS)/tmp

$(ROOTFS_BIN): | $(ROOTFS_DIR)

$(ROOTFS_DIR):
	@mkdir -p $@

setup: $(ROOTFS_BIN)

$(ROOTFS)/bin/abs-true:
	$(Q)ln -fs $$(which true) $@

$(ROOTFS)/bin/rel-true:
	$(Q)ln -fs ./true $@

$(ROOTFS)/bin/exec-m32: exec.c
	$(Q)$(CC) -m32 -static $^ -o $@ $(silently) || true

$(ROOTFS)/bin/execfn-m32: execfn.c
	$(Q)$(CC) -m32 -static $^ -o $@ $(silently) || true

$(ROOTFS)/bin/exec-suid: $(ROOTFS)/bin/exec
	$(Q)cp $^ $@
	$(Q)chmod u+s $@

$(ROOTFS)/bin/exec-sgid: $(ROOTFS)/bin/exec
	$(Q)cp $^ $@
	$(Q)chmod g+s $@

$(ROOTFS)/bin/exec-m32-suid: $(ROOTFS)/bin/exec-m32
	$(Q)cp $^     $@ $(silently) || true
	$(Q)chmod u+s $@ $(silently) || true

$(ROOTFS)/bin/exec-m32-sgid: $(ROOTFS)/bin/exec-m32
	$(Q)cp $^ $@      $(silently) || true
	$(Q)chmod g+s $@  $(silently) || true

.SECONDARY: $(patsubst %.c,$(ROOTFS)/bin/%, $(wildcard test-*.c))
$(ROOTFS)/bin/%: %.c
	$(Q)$(CC) -static $*.c -o $@ $(silently) || true

# Special cases.
test-rlimit-stack-brk: test-rlimit-stack-brk.c
	$(Q)$(CC) $< -o $@ $(silently) || true

# Not supported anymore.
# test-proc-self-cmdline-read: test-proc-self-cmdline-read.c
# 	$(Q)$(CC) $< -Wl,-rpath=foo -o $@ $(silently) || true

test-chdir-thread-fork: test-chdir-thread-fork.c
	$(Q)$(CC) $< -pthread -static -o $@ $(silently) || true

test-fork-vfork-execve: test-fork-vfork-execve.c
	$(Q)$(CC) $< -static -o $@ $(silently) || true

test-getresuid-getresgid-syscalls: test-getresuid-getresgid-syscalls.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-brk-syscall: test-brk-syscall.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-exec-proc-self-exe: test-exec-proc-self-exe.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-exec-proc-self-exe-null-argv: test-exec-proc-self-exe-null-argv.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-cwd-rename-rmdir: test-cwd-rename-rmdir.c
	$(Q)$(CC) $< -static -o $@ $(silently) || true

test-pipe2-nonblock: test-pipe2-nonblock.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-setuid-thread-vs-process: test-setuid-thread-vs-process.c
	$(Q)$(CC) $< -pthread -static -o $@ $(silently) || true

test-a3e68988: test-a3e68988.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-openat-fd-edge-cases: test-openat-fd-edge-cases.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-faccessat-nofollow-ignored: test-faccessat-nofollow-ignored.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-constructor-runs-once: test-constructor-runs-once.c
	$(Q)$(CC) -fPIE -pie $< -o $@ $(silently) || true

test-thread-execve-nyi: test-thread-execve-nyi.c
	$(Q)$(CC) $< -pthread -o $@ $(silently) || true

#test-bug-182: test-bug-182.c
#	$(Q)$(CC) $< -o $@ $(silently) || true

test-c47aeb7d: test-c47aeb7d.c
	$(Q)$(CC) $< -pthread -o $@ $(silently) || true

######################################################################
# Beautified output

V = 0
QUIET_LOG = /dev/null
ifeq ($(V), 0)
    quiet = quiet_
    Q     = @
    silently = >> $(QUIET_LOG) 2>&1
else
    quiet = 
    Q     = 
    silently = 
endif
