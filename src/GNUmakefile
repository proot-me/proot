# If you want to build outside of the source tree, use the -f option:
#     make -f ${SOMEWHERE}/proot/src/GNUmakefile

# the VPATH variable must points to the actual makefile directory
VPATH	:= $(dir $(lastword $(MAKEFILE_LIST)))
GIT      = git
RM       = rm
INSTALL  = install
CC       = gcc
LD       = $(CC)
CPPFLAGS = -I$(PWD)
CFLAGS   = -Wall -O0 -g
LDFLAGS  = 

OBJECTS = main.o tracee_info.o tracee_mem.o syscall.o path.o execve.o notice.o ureg.o interp.o

.DEFAULT_GOAL = proot

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

DEFAULT_VERSION = v0.6.2
CHECK_VERSION = VERSION=$$($(GIT) describe --tags --dirty --abbrev=8 --always 2>/dev/null); \
		if [ -z "$${VERSION}" ];                         \
		then echo "\#define VERSION $(DEFAULT_VERSION)"; \
		else echo "\#define VERSION $${VERSION}";        \
		fi;

.SILENT .IGNORE .INTERMEDIATE: .check_readlinkat .check_readlinkat.o

.check_readlinkat.o: .check_readlinkat.c
	$(COMPILE:echo=false) $(silently)

.check_readlinkat: .check_readlinkat.o
	$(LINK:echo=false) $(silently)

CHECK_READLINKAT = if [ -e .check_readlinkat ]; then echo "\#define HAVE_READLINKAT"; fi

config.h: .check_readlinkat
	$($(quiet)GEN)
	$(Q)echo "/* This file is auto-generated, edit at your own risk.  */" > $@
	$(Q)echo "#ifndef CONFIG_H"      >> $@
	$(Q)echo "#define CONFIG_H"      >> $@
	$(Q)sh -c '$(CHECK_VERSION)'     >> $@
	$(Q)sh -c '$(CHECK_READLINKAT)'  >> $@
	$(Q)echo "#endif /* CONFIG_H */" >> $@

######################################################################
# Build rules

SRC     = $(dir $(firstword $(MAKEFILE_LIST)))
COMPILE = $($(quiet)CC) $(CPPFLAGS) $(CFLAGS) -MD -c $(SRC)$*.c -o $@
LINK    = $($(quiet)LD) $(LDFLAGS) -o $@ $^

proot: $(OBJECTS)
	$(LINK)

# Special case to compute which files depend on the auto-generated
# file "config.h".
NEED_CONFIG := $(notdir $(patsubst %.c,%.o,$(shell egrep -l 'include[[:space:]]+"config.h"' $(SRC)*.c)))
$(NEED_CONFIG): config.h

%.o: %.c
	$(COMPILE)

######################################################################
# Dependencies

.DELETE_ON_ERROR:
$(OBJECTS) config.h: $(firstword $(MAKEFILE_LIST))

DEPS = $(OBJECTS:.o=.d) .check_readlinkat.d
-include $(DEPS)

######################################################################
# PHONY targets

PREFIX = /usr/local
DESTDIR = $(PREFIX)/bin

.PHONY: clean distclean install uninstall
clean distclean:
	-$(RM) -f $(OBJECTS) proot $(DEPS) config.h

install: proot
	$($(quiet)INSTALL) -D $< $(DESTDIR)/$<

uninstall:
	-$(RM) -f $(DESTDIR)/proot
