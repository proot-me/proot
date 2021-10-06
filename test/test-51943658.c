#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <string.h> /* strcmp */
#include <fcntl.h> /* openat(2), */

int main(void)
{
	int dir_fd;
	int dir_fd1;
	int dir_fd2;
	ssize_t status;
	char path1[PATH_MAX];
	char path2[PATH_MAX];
	char fd_link[64];

	/* Format the path to the "virtual" link. */

	dir_fd = open("/", O_RDONLY);
	if (dir_fd < 0) {
		perror("open(2)");
		exit(EXIT_FAILURE);
	}

	dir_fd1 = openat(dir_fd, ".", O_RDONLY);
	if (dir_fd1 < 0) {
		perror("openat(2)");
		exit(EXIT_FAILURE);
	}

	dir_fd2 = openat(dir_fd, "..", O_RDONLY);
	if (dir_fd2 < 0) {
		perror("openat(2)");
		exit(EXIT_FAILURE);
	}

	sprintf(fd_link, "/proc/self/fd/%d", dir_fd1);
	status = readlink(fd_link, path1, PATH_MAX - 1);
	if (status < 0) {
		perror("readlink(2)");
		exit(EXIT_FAILURE);
	}
	path1[status] = '\0';

	sprintf(fd_link, "/proc/self/fd/%d", dir_fd2);
	status = readlink(fd_link, path2, PATH_MAX - 1);
	if (status < 0) {
		perror("readlink(2)");
		exit(EXIT_FAILURE);
	}
	path2[status] = '\0';

	if (strcmp(path1, "/") != 0) {
		fprintf(stderr, "/. != /");
		exit(EXIT_FAILURE);
	}

	if (strcmp(path2, "/") != 0) {
		fprintf(stderr, "/.. != /");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
