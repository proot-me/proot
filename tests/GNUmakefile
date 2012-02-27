ROOTFS = ./rootfs
PROOT  = ../src/proot
CC     = gcc

TESTS_SH  = $(wildcard test-*.sh)
TESTS_C   = $(wildcard test-*.c)
TESTS_ALL = $(TESTS_SH) $(TESTS_C)

test check: | clean_failure check_failure

clean_failure:
	@rm -f failure

check_failure: $(TESTS_ALL)
	@bash -c '! test -e failure'

.PHONY: test check clean_failure check_failure setup test-%.sh test-%.c

test-%.sh: setup
	$(Q)env PROOT=$(PROOT) ROOTFS=$(ROOTFS) sh -e $@ $(silently); $(CHECK)

.SECONDARY: $(patsubst %.c,,$(TESTS_C))
test-%.c: setup $(ROOTFS)/bin/test-%
	$(Q)if [ -e $(ROOTFS)/bin/test-$* ]; then	\
		$(PROOT) -b /proc $(ROOTFS) /bin/test-$* $(silently); $(CHECK) \
	else						\
		echo "  TEST	$* skipped";		\
	fi

CHECK = case "$$?" in					\
		0)   echo "  TEST	$* ok";;	\
		125) echo "  TEST	$* skipped";;	\
		*)   echo "  TEST	$* FAILED";	\
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

$(ROOTFS)/bin/%: # %.c
	@test -e $(dir $@) || mkdir -p $(dir $@)
	$(Q)$(CC) -static $*.c -o $@ $(silently) || true

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
