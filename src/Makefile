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

quiet_GEN = @echo "  GEN	$(1)"; $(GEN)
quiet_DEP = @echo "  DEP	$@"; $(CC)
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

SRC = $(dir $(firstword $(MAKEFILE_LIST)))

GEN_HEADERS = version.h readlinkat.h

define generate =
  $(1): $(2)
	$($(quiet)GEN)
	$(Q)echo "/* This file is auto-generated, edit at your own risk.  */" > $(1)
	$(Q)echo "#ifndef $(3)" >> $(1)
	$(Q)echo "#define $(3)" >> $(1)
	$(Q)sh -c '$(value $(3))' >> $(1)
	$(Q)echo "#endif /* $(3) */" >> $(1)
endef

## version.h ##

DEFAULT_VERSION = v0.6.1+
GEN_VERSION_H = VERSION=$$($(GIT) describe --tags --dirty --abbrev=8 --always) 2>/dev/null; \
	if [ -z "$${VERSION}" ]; then echo "\#define VERSION $(DEFAULT_VERSION)"; else echo "\#define VERSION $${VERSION}"; fi;
$(eval $(call generate,version.h,,GEN_VERSION_H))

## readlinkat.h ##

.IGNORE .INTERMEDIATE: .check_readlinkat.o
.check_readlinkat.o:
	$(COMPILE)

GEN_READLINKAT_H = if [ -e .check_readlinkat.o ]; then echo "\#define HAVE_READLINKAT"; fi
$(eval $(call generate,readlinkat.h,.check_readlinkat.o,GEN_READLINKAT_H))

######################################################################
# Build rules

proot: $(OBJECTS)
	$($(quiet)LD) $(LDFLAGS) $^ -o $@

COMPILE = $($(quiet)CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC)$*.c -o $@

%.o: %.d
	$(COMPILE)

%.d: %.c
	$($(quiet)DEP) $(CPPFLAGS) $(CFLAGS) -M -MT '$*.o $*.d' -MG $(SRC)$*.c -MF $@

######################################################################
# Dependancies

DEPS = $(OBJECTS:.o=.d)

.DELETE_ON_ERROR:
$(OBJECTS) $(DEPS) $(GEN_HEADERS): Makefile

ifneq ($(MAKECMDGOALS), clean)
    ifneq (,$(filter clean, $(MAKECMDGOALS)))
        $(error The target "clean" shall be invoked separately)
    else
        -include $(DEPS)
    endif
endif

######################################################################
# PHONY targets

.PHONY: clean
clean:
	-$(RM) -f $(OBJECTS) proot $(DEPS) $(GEN_HEADERS)
