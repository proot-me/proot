#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Open FILE and print what readlink(2) returns for its /proc/self/fd
 * entry in a buffer of SIZE bytes, the way callers that start with a
 * small buffer read it.  */
int main(int argc, char *argv[])
{
    char buffer[PATH_MAX];
    char link[64];
    ssize_t length;
    size_t size;
    int fd;

    if (argc != 3)
	return 1;

    size = strtoul(argv[2], NULL, 10);
    if (size == 0 || size > sizeof(buffer))
	return 1;

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
	return 1;

    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    length = readlink(link, buffer, size);
    if (length < 0)
	return 1;

    printf("%.*s\n", (int) length, buffer);
    return 0;
}
