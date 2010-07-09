CC       = gcc
LD       = $(CC)
CPPFLAGS = -D`uname -m`
CFLAGS   = -Wall
LDFLAGS  =

OBJECTS = main.o syscall.o

proot: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

######################################################################
# Phony targets follow.

.PHONY: clean

clean:
	rm -f $(OBJECTS) proot *.gcno *.gcda *.info
