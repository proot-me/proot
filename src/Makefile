CC       = gcc
LD       = $(CC)
CPPFLAGS = -MD -DVERSION=$(VERSION)
CFLAGS   = -Wall -O0 -g
LDFLAGS  = 

OBJECTS = main.o child_info.o child_mem.o syscall.o path.o execve.o notice.o ureg.o interp.o

VERSION := $(shell git describe --tags --dirty --abbrev=8 --always 2>/dev/null)
ifndef VERSION
    VERSION := v0.6.1+
endif

proot: $(OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@

DEPS = $(patsubst %.o, %.d, $(OBJECTS))
-include $(DEPS)

$(OBJECTS): Makefile

.PHONY: clean
clean:
	rm -f $(OBJECTS) proot
