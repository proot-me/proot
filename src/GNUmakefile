# If you want to build outside of the source tree, use the -f option:
#     make -f ${SOMEWHERE}/proot/src/GNUmakefile

# the VPATH variable must point to the actual makefile directory
VPATH := $(dir $(lastword $(MAKEFILE_LIST)))
SRC    = $(dir $(firstword $(MAKEFILE_LIST)))

GIT      = git
RM       = rm
INSTALL  = install
CC       = $(CROSS_COMPILE)gcc
LD       = $(CC)
STRIP    = $(CROSS_COMPILE)strip
OBJCOPY  = $(CROSS_COMPILE)objcopy
OBJDUMP  = $(CROSS_COMPILE)objdump
PYTHON   = python3

HAS_SWIG := $(shell swig -version 2>/dev/null)
PYTHON_MAJOR_VERSION = $(shell ${PYTHON} -c "import sys; print(sys.version_info.major)" 2>/dev/null)
PYTHON_EMBED = $(shell ${PYTHON} -c "import sys; print('--embed' if sys.hexversion > 0x03080000 else '')" 2>/dev/null)
HAS_PYTHON_CONFIG := $(shell ${PYTHON}-config --ldflags ${PYTHON_EMBED} 2>/dev/null)

CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I$(VPATH) -I$(VPATH)/../lib/uthash/include
CFLAGS   += -g -Wall -Wextra -O2
CFLAGS   += $(shell pkg-config --cflags talloc)
LDFLAGS  += -Wl,-z,noexecstack
LDFLAGS  += $(shell pkg-config --libs talloc)

CARE_LDFLAGS  = $(shell pkg-config --libs libarchive)

OBJECTS += \
	cli/cli.o		\
	cli/proot.o		\
	cli/note.o		\
	execve/enter.o		\
	execve/exit.o		\
	execve/shebang.o	\
	execve/elf.o		\
	execve/ldso.o		\
	execve/auxv.o		\
	execve/aoxp.o		\
	path/binding.o		\
	path/glue.o		\
	path/canon.o		\
	path/path.o		\
	path/proc.o		\
	path/temp.o		\
	syscall/seccomp.o	\
	syscall/syscall.o	\
	syscall/chain.o		\
	syscall/enter.o		\
	syscall/exit.o		\
	syscall/sysnum.o	\
	syscall/socket.o	\
	syscall/heap.o		\
	syscall/rlimit.o	\
	tracee/tracee.o		\
	tracee/mem.o		\
	tracee/reg.o		\
	tracee/event.o		\
	ptrace/ptrace.o		\
	ptrace/user.o		\
	ptrace/wait.o		\
	extension/extension.o	\
	extension/kompat/kompat.o \
	extension/fake_id0/fake_id0.o \
	extension/link2symlink/link2symlink.o \
	extension/portmap/portmap.o \
	extension/portmap/map.o \
	loader/loader-wrapped.o

define define_from_arch.h
$2$1 := $(shell $(CC) $1 -E -dM -DNO_LIBC_HEADER $(SRC)/arch.h | grep -w $2 | cut -f 3 -d ' ')
endef

$(eval $(call define_from_arch.h,,HAS_LOADER_32BIT))

ifdef HAS_LOADER_32BIT
  OBJECTS += loader/loader-m32-wrapped.o
endif

ifneq ($(and $(HAS_SWIG),$(HAS_PYTHON_CONFIG)),)
  OBJECTS += extension/python/python.o \
		   extension/python/proot_wrap.o \
		   extension/python/python_extension.o \
		   extension/python/proot.o
endif

CARE_OBJECTS = 				\
	cli/care.o			\
	cli/care-manual.o		\
	extension/care/care.o		\
	extension/care/final.o		\
	extension/care/extract.o	\
	extension/care/archive.o

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

GIT_VERSION := $(shell git describe --tags `git rev-list --tags --max-count=1`)

GIT_COMMIT := $(shell git rev-list --all --max-count=1 | cut -c 1-8)

VERSION = $(GIT_VERSION)-$(GIT_COMMIT)

CHECK_VERSION = if [ ! -z "$(VERSION)" ]; \
                then /bin/echo -e "\#undef VERSION\n\#define VERSION \"$(VERSION)\""; \
                fi;

ifneq ($(and $(HAS_SWIG),$(HAS_PYTHON_CONFIG)),)
	CHECK_PYTHON_EXTENSION = /bin/echo -e "\#define HAVE_PYTHON_EXTENSION"
endif

CHECK_FEATURES = process_vm seccomp_filter
CHECK_PROGRAMS = $(foreach feature,$(CHECK_FEATURES),.check_$(feature))
CHECK_OBJECTS  = $(foreach feature,$(CHECK_FEATURES),.check_$(feature).o)
CHECK_RESULTS  = $(foreach feature,$(CHECK_FEATURES),.check_$(feature).res)

.SILENT .IGNORE .INTERMEDIATE: $(CHECK_OBJECTS) $(CHECK_PROGRAMS)

.check_%.o: .check_%.c
	-$(COMPILE:echo=false) $(silently)

.check_%: .check_%.o
	-$(LINK:echo=false) $(silently)

.check_%.res: .check_%
	$(Q)if [ -e $< ]; then echo "#define HAVE_$(shell echo $* | tr a-z A-Z)" > $@; else echo "" > $@; fi

build.h: $(CHECK_RESULTS)
	$($(quiet)GEN)
	$(Q)echo "/* This file is auto-generated, edit at your own risk.  */" > $@
	$(Q)echo "#ifndef BUILD_H"      >> $@
	$(Q)echo "#define BUILD_H"      >> $@
	$(Q)sh -c '$(CHECK_VERSION)'    >> $@
	$(Q)sh -c '$(CHECK_PYTHON_EXTENSION)'    >> $@
	$(Q)cat $^                      >> $@
	$(Q)echo "#endif /* BUILD_H */" >> $@

BUILD_ID_NONE := $(shell if ld --build-id=none --version >/dev/null 2>&1; then echo ',--build-id=none'; fi)

######################################################################
# Build rules

COMPILE = $($(quiet)CC) $(CPPFLAGS) $(CFLAGS) -MD -c $(SRC)$< -o $@
LINK    = $($(quiet)LD) -o $@ $^ $(LDFLAGS)

OBJIFY = $($(quiet)GEN)									\
	$(OBJCOPY)									\
		--input-target binary							\
		--output-target `env LC_ALL=C $(OBJDUMP) -f cli/cli.o |			\
			grep 'file format' | awk '{print $$4}'`				\
		--binary-architecture `env LC_ALL=C $(OBJDUMP) -f cli/cli.o |		\
				grep architecture | cut -f 1 -d , | awk '{print $$2}'`	\
		$< $@

proot: $(OBJECTS)
	$(LINK)

care: $(OBJECTS) $(CARE_OBJECTS)
	$(LINK) $(CARE_LDFLAGS)

# Special case to compute which files depend on the auto-generated
# file "build.h".
USE_BUILD_H := $(patsubst $(SRC)%.c,%.o,$(shell grep -E -sl 'include[[:space:]]+"build.h"' $(patsubst %.o,$(SRC)%.c,$(OBJECTS) $(CARE_OBJECTS))))
$(USE_BUILD_H): build.h

%.o: %.c
	@mkdir -p $(dir $@)
	$(COMPILE)

.INTERMEDIATE: manual
manual: $(VPATH)/../doc/care/manual.rst
	$(Q)cp $< $@

cli/care-manual.o: manual cli/cli.o
	$(OBJIFY)

cli/%-licenses.o: licenses cli/cli.o
	$(OBJIFY)

######################################################################
# Python extension

define build_python_extension
CPPFLAGS 	+= $(shell ${PYTHON}-config --includes)
LDFLAGS 	+= $(shell ${PYTHON}-config --ldflags ${PYTHON_EMBED})
SWIG     	= swig
quiet_SWIG 	= @echo "  SWIG	$$@"; swig
SWIG_OPT 	= -python

ifeq ($(PYTHON_MAJOR_VERSION), 3)
	SWIG_OPT += -py3
endif

.INTERMEDIATE:python_extension.py
python_extension.py: extension/python/python_extension.py
	$$(Q)cp $$< $$@
extension/python/python_extension.o: python_extension.py cli/cli.o
	$$(OBJIFY)

.SECONDARY: proot_wrap.c proot.py
proot_wrap.c proot.py: extension/python/proot.i
	$$($$(quiet)SWIG) $$(SWIG_OPT) -outcurrentdir -I$$(VPATH) $$(VPATH)/extension/python/proot.i

extension/python/proot.o: proot.py cli/cli.o
	$$(OBJIFY)

extension/python/proot_wrap.o: proot_wrap.c
	$$($$(quiet)CC) $$(CPPFLAGS) $$(CFLAGS) -MD -c $$< -o $$@

endef

ifneq ($(and $(HAS_SWIG),$(HAS_PYTHON_CONFIG)),)
$(eval $(build_python_extension))
endif

######################################################################
# Build rules for the loader

define build_loader
LOADER$1_OBJECTS = loader/loader$1.o loader/assembly$1.o

$(eval $(call define_from_arch.h,$1,LOADER_ARCH_CFLAGS))
$(eval $(call define_from_arch.h,$1,LOADER_ADDRESS))

LOADER_CFLAGS$1  += -fPIC -ffreestanding $(LOADER_ARCH_CFLAGS$1)
LOADER_LDFLAGS$1 += -static -nostdlib -Wl$(BUILD_ID_NONE),-Ttext=$(LOADER_ADDRESS$1),-z,noexecstack

loader/loader$1.o: loader/loader.c
	@mkdir -p $$(dir $$@)
	$$(COMPILE) $1 $$(LOADER_CFLAGS$1)

loader/assembly$1.o: loader/assembly.S
	@mkdir -p $$(dir $$@)
	$$(COMPILE) $1 $$(LOADER_CFLAGS$1)

loader/loader$1: $$(LOADER$1_OBJECTS)
	$$($$(quiet)LD) $1 -o $$@ $$^ $$(LOADER_LDFLAGS$1)

.INTERMEDIATE: loader$1.elf
loader$1.elf: loader/loader$1
	$$(Q)cp $$< $$@
	$$(Q)$(STRIP) $$@

loader/loader$1-wrapped.o: loader$1.elf cli/cli.o
	$$(OBJIFY)

endef

$(eval $(build_loader))

ifdef HAS_LOADER_32BIT
$(eval $(call build_loader,-m32))
endif

######################################################################
# Dependencies

.DELETE_ON_ERROR:
$(OBJECTS) $(CARE_OBJECTS) $(LOADER_OBJECTS) $(LOADER-m32_OBJECTS): $(firstword $(MAKEFILE_LIST))

DEPS = $(OBJECTS:.o=.d) $(CARE_OBJECTS:.o=.d) $(LOADER_OBJECTS:.o=.d) $(LOADER-m32_OBJECTS:.o=.d) $(CHECK_OBJECTS:.o=.d)
-include $(DEPS)

######################################################################
# PHONY targets

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: clean distclean install install-care uninstall
clean distclean:
	-$(RM) -f $(CHECK_OBJECTS) $(CHECK_PROGRAMS) $(CHECK_RESULTS) $(OBJECTS) $(CARE_OBJECTS) $(LOADER_OBJECTS) $(LOADER-m32_OBJECTS) proot care loader/loader loader/loader-m32 cli/care-manual.o $(DEPS) build.h licenses proot.py proot_wrap.c

install: proot
	$($(quiet)INSTALL) -D $< $(DESTDIR)$(BINDIR)/$<

install-care: care
	$($(quiet)INSTALL) -D $< $(DESTDIR)$(BINDIR)/$<

uninstall:
	-$(RM) -f $(DESTDIR)$(BINDIR)/proot

uninstall-care:
	-$(RM) -f $(DESTDIR)$(BINDIR)/care
