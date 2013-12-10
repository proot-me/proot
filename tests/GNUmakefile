DIR    = $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
ROOTFS = $(DIR)/rootfs
PROOT  = $(DIR)/../src/proot
CARE   = $(DIR)/../src/care
CC     = gcc
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
	$(Q)env CARE="$(CARE)" PROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) sh -ex $< $(silently); $(call check,$*)

check-%.c: $(ROOTFS)/bin/% setup
	$(call check_c,$*,$(PROOT) -b /proc -r $(ROOTFS) /bin/$*)

# Special cases.
check-test-bdc90417.c: test-bdc90417
	$(call check_c,$<,$(PROOT) -w . / ./$<)

check-test-af062114.c: test-af062114
	$(call check_c,$<,$(PROOT) -v -1 -q /bin/true -b . -b /lib -b /lib64 -b /proc $(ROOTFS) ./$< | grep -- --inhibit-rpath)
#	$(call check_c,$<,$(PROOT) -v -1 -q /bin/true / ./$< | grep '^./$<') deprecated

check-test-5bed7141.c: test-5bed7141
	$(call check_c,$<,$(PROOT) ./$<)

check-test-5bed7143.c: test-5bed7143
	$(call check_c,$<,$(PROOT) -r $(ROOTFS) -b . ./$<)
	$(call check_c,$<,$(PROOT) ./$<)

check-test-16573e73.c: test-16573e73
	$(call check_c,$<,$(PROOT) ./$<)
	$(call check_c,$<,$(PROOT) ./$< 1)

check-test-82ba4ba1.c: test-82ba4ba1
	$(call check_c,$<,$(PROOT) -0 ./$<)

check-test-kkkkkkkk.c: test-kkkkkkkk
	$(call check_c,$<,$(PROOT) ./$<)

check-test-25069c12.c: test-25069c12
	$(call check_c,$<,$(PROOT) ./$<)

check-test-25069c13.c: test-25069c13
	$(call check_c,$<,$(PROOT) ./$<)

check-test-1ffc8309.c: test-1ffc8309
	$(call check_c,$<,env PROOT_FORCE_KOMPAT=1 $(PROOT) -k $(shell uname -r) ./$<)

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

setup: $(ROOTFS)/bin/true $(ROOTFS)/bin/false    \
       $(ROOTFS)/bin/pwd  $(ROOTFS)/bin/readlink $(ROOTFS)/bin/symlink \
       $(ROOTFS)/bin/abs-true $(ROOTFS)/bin/rel-true $(ROOTFS)/bin/echo \
       $(ROOTFS)/bin/argv0 $(ROOTFS)/bin/readdir $(ROOTFS)/bin/cat $(ROOTFS)/tmp \
       $(ROOTFS)/bin/chdir_getcwd $(ROOTFS)/bin/fchdir_getcwd

$(ROOTFS)/tmp:
	@mkdir $@

$(ROOTFS)/bin/abs-true:
	@ln -fs /bin/true $@

$(ROOTFS)/bin/rel-true:
	@ln -fs ./true $@

.SECONDARY: $(patsubst %.c,$(ROOTFS)/bin/%, $(wildcard test-*.c))
$(ROOTFS)/bin/%: %.c
	@test -e $(dir $@) || mkdir -p $(dir $@)
	$(Q)$(CC) -static $*.c -o $@ $(silently) || true

# Special cases.
test-bdc90417: test-bdc90417.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-af062114: test-af062114.c
	$(Q)$(CC) $< -Wl,-rpath=foo -o $@ $(silently) || true

test-5bed7141: test-5bed7141.c
	$(Q)$(CC) $< -pthread -static -o $@ $(silently) || true

test-16573e73: test-16573e73.c
	$(Q)$(CC) $< -static -o $@ $(silently) || true

test-82ba4ba1: test-82ba4ba1.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-kkkkkkkk: test-kkkkkkkk.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-25069c12: test-25069c12.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-25069c13: test-25069c13.c
	$(Q)$(CC) $< -o $@ $(silently) || true

test-5bed7143: test-5bed7143.c
	$(Q)$(CC) $< -static -o $@ $(silently) || true

test-1ffc8309: test-1ffc8309.c
	$(Q)$(CC) $< -o $@ $(silently) || true

######################################################################
# Beautified output

V = 0
ifeq ($(V), 0)
    quiet = quiet_
    Q     = @
    silently = >/dev/null 2>&1
else
    quiet = 
    Q     = 
    silently = 
endif
