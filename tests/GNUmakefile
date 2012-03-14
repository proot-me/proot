ROOTFS = ./rootfs
PROOT  = ../src/proot
CC     = gcc

CHECK_TESTS = $(patsubst %,check-%, $(wildcard test-*.sh) $(wildcard test-*.c))

.PHONY: check clean_failure check_failure setup check-%

check: | clean_failure check_failure

clean_failure:
	@rm -f failure

check_failure: $(CHECK_TESTS)
	@bash -c '! test -e failure'

check-%.sh: %.sh setup
	$(Q)env PROOT=$(PROOT) ROOTFS=$(ROOTFS) sh -e $< $(silently); $(call check,$*)

check-%.c: $(ROOTFS)/bin/% setup
	$(call check_c,$*,$(PROOT) -b /proc $(ROOTFS) /bin/$*)

# Special case.
check-test-bdc90417.c: test-bdc90417
	$(call check_c,$<,$(PROOT) -w . / $<)

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
       $(ROOTFS)/bin/pwd  $(ROOTFS)/bin/readlink \
       $(ROOTFS)/bin/abs-true $(ROOTFS)/bin/rel-true

$(ROOTFS)/bin/abs-true:
	@ln -fs /bin/true $@

$(ROOTFS)/bin/rel-true:
	@ln -fs ./true $@

.SECONDARY: $(patsubst %.c,$(ROOTFS)/bin/%, $(wildcard test-*.c))
$(ROOTFS)/bin/%: %.c
	@test -e $(dir $@) || mkdir -p $(dir $@)
	$(Q)$(CC) -static $*.c -o $@ $(silently) || true

# Special case.
test-bdc90417: test-bdc90417.c
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
