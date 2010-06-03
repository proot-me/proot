CC = gcc
LD = $(CC)
CFLAGS = 

OBJECTS = proot.o test.o

proot: $(OBJECTS)
	$(LD) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -f $(OBJECTS) proot