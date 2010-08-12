CC       = gcc
LD       = $(CC)
CPPFLAGS = -D$(ARCH)=1
CFLAGS   = -Wall -O0 -g
LDFLAGS  =

ARCH     = $(shell uname -m)

OBJECTS = main.o child_info.o child_mem.o syscall.o path.o execve.o notice.o

all: proot proot-exec

proot: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

proot-exec: proot-exec.o
	$(LD) $(LDFLAGS) $? -static -o $@

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

######################################################################
# Phony targets follow.

.PHONY: clean

clean:
	rm -f $(OBJECTS) proot-exec.o proot-exec proot *.gcno *.gcda *.info
