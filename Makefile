CC       = gcc
LD       = $(CC)
CPPFLAGS = 
CFLAGS   = -Wall -O0 -g
LDFLAGS  = -static

OBJECTS = main.o child_info.o child_mem.o syscall.o path.o execve.o notice.o arch.o

VERSION := $(shell git describe --dirty --abbrev=8 --always 2>/dev/null)
ifndef VERSION
    VERSION := beta2?
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
