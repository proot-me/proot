# If you want to build outside of the source tree, use the -f option:
#     make -f ${SOMEWHERE}/proot/src/GNUmakefile

# the VPATH variable must point to the actual makefile directory
VPATH	:= $(dir $(lastword $(MAKEFILE_LIST)))
GIT      = git
RM       = rm
INSTALL  = install
CC       = gcc
LD       = $(CC)
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I$(VPATH)
CFLAGS   += -Wall -Wextra -O2
LDFLAGS  += -ltalloc

OBJECTS += \
	cli/cli.o		\
	cli/proot.o		\
	cli/notice.o		\
	execve/execve.o		\
	execve/interp.o		\
	execve/elf.o		\
	execve/ldso.o		\
	path/binding.o		\
	path/glue.o		\
	path/canon.o		\
	path/path.o		\
	path/proc.o		\
	syscall/seccomp.o	\
	syscall/syscall.o	\
	syscall/chain.o		\
	syscall/enter.o		\
	syscall/exit.o		\
	syscall/sysnum.o	\
	syscall/socket.o	\
	syscall/heap.o		\
	tracee/tracee.o		\
	tracee/array.o		\
	tracee/mem.o		\
	tracee/reg.o		\
	tracee/event.o		\
	extension/extension.o	\
	extension/kompat/kompat.o \
	extension/fake_id0/fake_id0.o

ifdef CARE

OBJECTS +=				\
	cli/care.o			\
	cli/care-manual.o		\
	extension/care/care.o		\
	extension/care/final.o		\
	extension/care/archive.o

.INTERMEDIATE: manual
manual: $(VPATH)/../doc/care-manual.txt
	$(Q)cp $< $@

cli/care-manual.o: manual cli/cli.o
	$(OBJIFY)

LDFLAGS += -larchive

.DEFAULT_GOAL = care
all: care

else

.DEFAULT_GOAL = proot
all: proot

endif

cli/%-licenses.o: licenses cli/cli.o
	$(OBJIFY)

OBJIFY = $($(quiet)GEN)									\
	objcopy										\
		--input binary								\
		--output `objdump -f cli/cli.o |					\
			  perl -pe '/file format (.*)$$/; $$_ = $$1'`			\
		--binary-architecture `objdump -f cli/cli.o |				\
				       perl -pe '/architecture: (\w+).*/; $$_ = $$1'`	\
		$< $@

######################################################################
# Beautified output

quiet_GEN = @echo "  GEN	$@"; $(GEN)
quiet_CC  = @echo "  CC	$@"; $(CC)
quiet_LD  = @echo "  LD	$@"; $(LD)
quiet_INSTALL = @echo "  INSTALL	$?"; $(INSTALL)

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

######################################################################
# Auto-configuration

CHECK_VERSION = VERSION=$$($(GIT) describe --tags --dirty --abbrev=8 --always 2>/dev/null); \
		if [ ! -z "$${VERSION}" ]; \
		then /bin/echo -e "\#undef VERSION\n\#define VERSION \"$${VERSION}\""; \
		fi;

CHECK_FEATURES = process_vm seccomp_filter
CHECK_PROGRAMS = $(foreach feature,$(CHECK_FEATURES),.check_$(feature))
CHECK_OBJECTS  = $(foreach feature,$(CHECK_FEATURES),.check_$(feature).o)
CHECK_RESULTS  = $(foreach feature,$(CHECK_FEATURES),.check_$(feature).res)

.SILENT .IGNORE .INTERMEDIATE: $(CHECK_OBJECTS) $(CHECK_PROGRAMS)

.check_%.o: .check_%.c
	$(COMPILE:echo=false) $(silently) || true

.check_%: .check_%.o
	$(LINK:echo=false) $(silently) || true

.check_%.res: .check_%
	$(Q)if [ -e $< ]; then echo "#define HAVE_$(shell echo $* | tr a-z A-Z)" > $@; else echo "" > $@; fi

build.h: $(CHECK_RESULTS)
	$($(quiet)GEN)
	$(Q)echo "/* This file is auto-generated, edit at your own risk.  */" > $@
	$(Q)echo "#ifndef BUILD_H"      >> $@
	$(Q)echo "#define BUILD_H"      >> $@
	$(Q)sh -c '$(CHECK_VERSION)'    >> $@
	$(Q)cat $^                      >> $@
	$(Q)echo "#endif /* BUILD_H */" >> $@

######################################################################
# Build rules

SRC     = $(dir $(firstword $(MAKEFILE_LIST)))
COMPILE = $($(quiet)CC) $(CPPFLAGS) $(CFLAGS) -MD -c $(SRC)$(@:.o=.c) -o $@
LINK    = $($(quiet)LD) -o $@ $^ $(LDFLAGS)

proot care: $(OBJECTS)
	$(LINK)

# Special case to compute which files depend on the auto-generated
# file "build.h".
USE_BUILD_H := $(patsubst $(SRC)%,%,$(patsubst %.c,%.o,$(shell egrep -l 'include[[:space:]]+"build.h"' $(SRC)*/*.c)))
$(USE_BUILD_H): build.h

%.o: %.c
	@mkdir -p $(dir $@)
	$(COMPILE)

######################################################################
# Dependencies

.DELETE_ON_ERROR:
$(OBJECTS): $(firstword $(MAKEFILE_LIST))

DEPS = $(OBJECTS:.o=.d) $(CHECK_OBJECTS:.o=.d)
-include $(DEPS)

######################################################################
# PHONY targets

PREFIX = /usr/local
DESTDIR = $(PREFIX)/bin

.PHONY: clean distclean install uninstall
clean distclean:
	-$(RM) -f $(CHECK_OBJECTS) $(CHECK_PROGRAMS) $(CHECK_RESULTS) $(OBJECTS) proot care cli/care-manual.o $(DEPS) build.h

install: proot
	$($(quiet)INSTALL) -D $< $(DESTDIR)/$<

uninstall:
	-$(RM) -f $(DESTDIR)/proot
