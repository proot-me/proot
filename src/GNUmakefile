# If you want to build outside of the source tree, use the -f option:
#     make -f ${SOMEWHERE}/proot/src/GNUmakefile

# the VPATH variable must points to the actual makefile directory
VPATH	:= $(dir $(lastword $(MAKEFILE_LIST)))
GIT      = git
RM       = rm
INSTALL  = install
CC       = gcc
LD       = $(CC)
CPPFLAGS = -I. -I$(VPATH)
CFLAGS   = -Wall -O2
LDFLAGS  = 

OBJECTS = cli.o			\
	config.o		\
	notice.o		\
	execve/execve.o		\
	execve/interp.o		\
	execve/elf.o		\
	execve/ldso.o		\
	path/binding.o		\
	path/canon.o		\
	path/path.o		\
	path/proc.o		\
	syscall/syscall.o	\
	tracee/tracee.o		\
	tracee/array.o		\
	tracee/mem.o		\
	tracee/reg.o		\
	tracee/event.o

.DEFAULT_GOAL = proot
all: proot

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

CHECK_OBJECTS = .check_readlinkat .check_readlinkat.o .check_process_vm .check_process_vm.o
.SILENT .IGNORE .INTERMEDIATE: $(CHECK_OBJECTS)

.check_readlinkat.o: .check_readlinkat.c
	$(COMPILE:echo=false) $(silently) || true

.check_readlinkat: .check_readlinkat.o
	$(LINK:echo=false) $(silently) || true

CHECK_READLINKAT = if [ -e .check_readlinkat ]; then echo "\#define HAVE_READLINKAT"; fi

.check_process_vm.o: .check_process_vm.c
	$(COMPILE:echo=false) $(silently) || true

.check_process_vm: .check_process_vm.o
	$(LINK:echo=false) $(silently) || true

CHECK_PROCESS_VM = if [ -e .check_process_vm ]; then echo "\#define HAVE_PROCESS_VM"; fi

build.h: .check_readlinkat .check_process_vm
	$($(quiet)GEN)
	$(Q)echo "/* This file is auto-generated, edit at your own risk.  */" > $@
	$(Q)echo "#ifndef BUILD_H"      >> $@
	$(Q)echo "#define BUILD_H"      >> $@
	$(Q)sh -c '$(CHECK_VERSION)'     >> $@
	$(Q)sh -c '$(CHECK_READLINKAT)'  >> $@
	$(Q)sh -c '$(CHECK_PROCESS_VM)'  >> $@
	$(Q)echo "#endif /* BUILD_H */" >> $@

######################################################################
# Build rules

SRC     = $(dir $(firstword $(MAKEFILE_LIST)))
COMPILE = $($(quiet)CC) $(CPPFLAGS) $(CFLAGS) -MD -c $(SRC)$*.c -o $@
LINK    = $($(quiet)LD) -o $@ $^ $(LDFLAGS)

proot: $(OBJECTS)
	$(LINK)

# Special case to compute which files depend on the auto-generated
# file "build.h".
USE_BUILD_H := $(patsubst $(SRC)%,%,$(patsubst %.c,%.o,$(shell egrep -l 'include[[:space:]]+"build.h"' $(SRC)*.c $(SRC)*/*.c)))
$(USE_BUILD_H): build.h

%.o: %.c
	@mkdir -p $(dir $@)
	$(COMPILE)

######################################################################
# Dependencies

.DELETE_ON_ERROR:
$(OBJECTS) build.h: $(firstword $(MAKEFILE_LIST))

DEPS = $(OBJECTS:.o=.d) .check_readlinkat.d .check_process_vm.d
-include $(DEPS)

######################################################################
# PHONY targets

PREFIX = /usr/local
DESTDIR = $(PREFIX)/bin

.PHONY: clean distclean install uninstall
clean distclean:
	-$(RM) -f $(CHECK_OBJECTS) $(OBJECTS) proot $(DEPS) build.h

install: proot
	$($(quiet)INSTALL) -D $< $(DESTDIR)/$<

uninstall:
	-$(RM) -f $(DESTDIR)/proot
