GIT      = git
RM       = rm
DEP      = $(CC)
CC       = gcc
LD       = $(CC)
CPPFLAGS = -I$(PWD)
CFLAGS   = -Wall -O0 -g
LDFLAGS  = 

OBJECTS = main.o child_info.o child_mem.o syscall.o path.o execve.o notice.o ureg.o interp.o

all: proot

######################################################################
# Beautified output

quiet_GEN = @echo "  GEN	$@"; $(GEN)
quiet_CC  = @echo "  CC	$@"; $(CC)
quiet_LD  = @echo "  LD	$@"; $(LD)

VERBOSE = 0
ifeq ($(VERBOSE), 0)
    quiet = quiet_
    Q     = @
else
    quiet = 
    Q     = 
endif

######################################################################
# Auto-configuration

DEFAULT_VERSION = v0.6.1
CHECK_VERSION = VERSION=$$($(GIT) describe --tags --dirty --abbrev=8 --always) 2>/dev/null; \
		if [ -z "$${VERSION}" ];                         \
		then echo "\#define VERSION $(DEFAULT_VERSION)"; \
		else echo "\#define VERSION $${VERSION}";        \
		fi;

.IGNORE .INTERMEDIATE: .check_readlinkat .check_readlinkat.o
.check_readlinkat: .check_readlinkat.o
	$(LINK)

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

# A little bit overkill since only main.c and path.c depend on
# config.h as of now.
$(OBJECTS): config.h

%.o: %.c
	$(COMPILE)

######################################################################
# Dependencies

.DELETE_ON_ERROR:
$(OBJECTS) config.h: Makefile

DEPS = $(OBJECTS:.o=.d)
-include $(DEPS)

######################################################################
# PHONY targets

.PHONY: clean
clean:
	-$(RM) -f $(OBJECTS) proot $(DEPS) config.h
