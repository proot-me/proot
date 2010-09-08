CC       = gcc
LD       = $(CC)
CPPFLAGS = 
CFLAGS   = -Wall -O0 -g
LDFLAGS  = -static

OBJECTS = main.o child_info.o child_mem.o syscall.o path.o execve.o notice.o

all: proot

proot: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c *.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

######################################################################
# Phony targets follow.

.PHONY: clean

clean:
	rm -f $(OBJECTS) proot *.gcno *.gcda *.info
