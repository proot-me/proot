#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_readlink, */

int main(int argc, char *argv[])
{
	char path[PATH_MAX];
	int status;

	if (argc != 2) {
		fprintf(stderr, "usage: readlink FILE\n");
		exit(EXIT_FAILURE);
	}

	status = syscall(SYS_readlink, argv[1], path, PATH_MAX);
	if (status < 0) {
		perror("readlink()");
		exit(EXIT_FAILURE);
	}
	path[status] = '\0';

	puts(path);

	exit(EXIT_SUCCESS);
}
