CC       = gcc
LD       = $(CC)
CPPFLAGS = 
CFLAGS   = -Wall -O0 -g
LDFLAGS  = 

OBJECTS = main.o child_info.o child_mem.o syscall.o path.o execve.o notice.o ureg.o interp.o

VERSION := $(shell git describe --tags --dirty --abbrev=8 --always 2>/dev/null)
ifndef VERSION
    VERSION := v0.5-30+
endif

all: proot

proot: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c *.h Makefile
	$(CC) -DVERSION=$(VERSION) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

######################################################################
# Phony targets follow.

.PHONY: clean

clean:
	rm -f $(OBJECTS) proot *.gcno *.gcda *.info
