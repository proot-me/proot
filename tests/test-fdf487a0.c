#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

int main()
{
	int fd;

	fd = openat(0, "foo", O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR) {
		printf("1. %d %d\n", fd, (int) errno);
		exit(EXIT_FAILURE);
	}

	fd = openat(0, "", O_RDONLY);
	if (fd >= 0 || errno != ENOENT) {
		printf("2. %d %d\n", fd, (int) errno);
		exit(EXIT_FAILURE);
	}

	fd = openat(0, NULL, O_RDONLY);
	if (fd >= 0 || errno != EFAULT) {
		printf("3. %d %d\n", fd, (int) errno);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
