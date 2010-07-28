#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <fcntl.h> /* openat(2), */

int main(void)
{
	int dir_fd;

	dir_fd = open("/", O_RDONLY);
	if (dir_fd < 0) {
		perror("open(2)");
		exit(EXIT_FAILURE);
	}

	if (openat(dir_fd, ".", O_RDONLY) < 0) {
		perror("openat(2)");
		exit(EXIT_FAILURE);
	}

	if (openat(dir_fd, "..", O_RDONLY) >= 0) {
		perror("openat(2): you should not pass!");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
