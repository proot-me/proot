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

CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I$(VPATH)
CFLAGS   += -Wall -Wextra -O2
LDFLAGS  += -ltalloc

CARE_LDFLAGS = -larchive

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
	loader/loader-wrapped.o

define define_from_arch.h
$2$1 := $(shell $(CC) $1 -E -dM -DNO_LIBC_HEADER $(SRC)/arch.h | grep -w $2 | cut -f 3 -d ' ')
endef

$(eval $(call define_from_arch.h,,HAS_LOADER_32BIT))

ifdef HAS_LOADER_32BIT
  OBJECTS += loader/loader-m32-wrapped.o
endif

CARE_OBJECTS = 				\
	cli/care.o			\
	cli/care-manual.o		\
	extension/care/care.o		\
	extension/care/final.o		\
	extension/care/extract.o	\
	extension/care/archive.o

WIOM_OBJECTS = 				\
	cli/wiom.o			\
	extension/wiom/wiom.o		\
	extension/wiom/event.o		\
	extension/wiom/format/binary.o	\
	extension/wiom/format/text.o	\
	extension/wiom/format/sio.o

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
	$(Q)cat $^                      >> $@
	$(Q)echo "#endif /* BUILD_H */" >> $@

BUILD_ID_NONE := $(shell if ld --build-id=none --version >/dev/null 2>&1; then echo ',--build-id=none'; fi)

######################################################################
# Build rules

COMPILE = $($(quiet)CC) $(CPPFLAGS) $(CFLAGS) -MD -c $(SRC)$< -o $@
LINK    = $($(quiet)LD) -o $@ $^ $(LDFLAGS)

OBJIFY = $($(quiet)GEN)									\
	$(OBJCOPY)									\
		--input binary								\
		--output `env LANG=C $(OBJDUMP) -f cli/cli.o |				\
			grep 'file format' | awk '{print $$4}'`				\
		--binary-architecture `env LANG=C $(OBJDUMP) -f cli/cli.o |		\
				grep architecture | cut -f 1 -d , | awk '{print $$2}'`	\
		$< $@

proot: $(OBJECTS)
	$(LINK)

care: $(OBJECTS) $(CARE_OBJECTS)
	$(LINK) $(CARE_LDFLAGS)

wiom: $(OBJECTS) $(WIOM_OBJECTS)
	$(LINK)

# Special case to compute which files depend on the auto-generated
# file "build.h".
USE_BUILD_H := $(patsubst $(SRC)%.c,%.o,$(shell egrep -sl 'include[[:space:]]+"build.h"' $(patsubst %.o,$(SRC)%.c,$(OBJECTS) $(CARE_OBJECTS) $(WIOM_OBJECTS))))
$(USE_BUILD_H): build.h

%.o: %.c
	@mkdir -p $(dir $@)
	$(COMPILE)

.INTERMEDIATE: manual
manual: $(VPATH)/../doc/care/manual.txt
	$(Q)cp $< $@

cli/care-manual.o: manual cli/cli.o
	$(OBJIFY)

cli/%-licenses.o: licenses cli/cli.o
	$(OBJIFY)

######################################################################
# Build rules for the loader

define build_loader
LOADER$1_OBJECTS = loader/loader$1.o loader/assembly$1.o

$(eval $(call define_from_arch.h,$1,LOADER_ARCH_CFLAGS))
$(eval $(call define_from_arch.h,$1,LOADER_ADDRESS))

LOADER_CFLAGS$1  += -fPIC -ffreestanding $(LOADER_ARCH_CFLAGS$1)
LOADER_LDFLAGS$1 += -static -nostdlib -Wl$(BUILD_ID_NONE),-Ttext=$(LOADER_ADDRESS$1)

loader/loader$1.o: loader/loader.c
	@mkdir -p $$(dir $$@)
	$$(COMPILE) $1 $$(LOADER_CFLAGS$1)

loader/assembly$1.o: loader/assembly.S
	@mkdir -p $$(dir $$@)
	$$(COMPILE) $1 $$(LOADER_CFLAGS$1)

loader/loader$1: $$(LOADER$1_OBJECTS)
	$$($$(quiet)LD) $1 -o $$@ $$^ $$(LOADER_LDFLAGS$1)

.INTERMEDIATE: loader$1.exe
loader$1.exe: loader/loader$1
	$$(Q)cp $$< $$@
	$$(Q)$(STRIP) $$@

loader/loader$1-wrapped.o: loader$1.exe cli/cli.o
	$$(OBJIFY)
endef

$(eval $(build_loader))

ifdef HAS_LOADER_32BIT
$(eval $(call build_loader,-m32))
endif

######################################################################
# Dependencies

.DELETE_ON_ERROR:
$(OBJECTS) $(CARE_OBJECTS) $(WIOM_OBJECTS) $(LOADER_OBJECTS) $(LOADER-m32_OBJECTS): $(firstword $(MAKEFILE_LIST))

DEPS = $(OBJECTS:.o=.d) $(CARE_OBJECTS:.o=.d) $(WIOM_OBJECTS:.o=.d) $(LOADER_OBJECTS:.o=.d) $(LOADER-m32_OBJECTS:.o=.d) $(CHECK_OBJECTS:.o=.d)
-include $(DEPS)

######################################################################
# PHONY targets

PREFIX = /usr/local
DESTDIR = $(PREFIX)/bin

.PHONY: clean distclean install install-care uninstall
clean distclean:
	-$(RM) -f $(CHECK_OBJECTS) $(CHECK_PROGRAMS) $(CHECK_RESULTS) $(OBJECTS) $(CARE_OBJECTS) $(WIOM_OBJECTS) $(LOADER_OBJECTS) $(LOADER-m32_OBJECTS) proot care loader/loader loader/loader-m32 cli/care-manual.o $(DEPS) build.h licenses

install: proot
	$($(quiet)INSTALL) -D $< $(DESTDIR)/$<

install-care: care
	$($(quiet)INSTALL) -D $< $(DESTDIR)/$<

uninstall:
	-$(RM) -f $(DESTDIR)/proot

uninstall-care:
	-$(RM) -f $(DESTDIR)/care
