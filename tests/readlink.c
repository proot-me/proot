#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_readlink, */
#include <fcntl.h> /* AT_FDCWD */

int main(int argc, char *argv[])
{
	char path[PATH_MAX];
	int status;

	if (argc != 2) {
		fprintf(stderr, "usage: readlink FILE\n");
		exit(EXIT_FAILURE);
	}

#if defined(SYS_readlink)
	status = syscall(SYS_readlink, argv[1], path, PATH_MAX);
#elif defined(SYS_readlinkat)
	status = syscall(SYS_readlinkat, AT_FDCWD, argv[1], path, PATH_MAX);
#else
#error "SYS_readlink and SYS_readlinkat doesn't exists"
#endif
	if (status < 0) {
		perror("readlink()");
		exit(EXIT_FAILURE);
	}
	path[status] = '\0';

	puts(path);

	exit(EXIT_SUCCESS);
}
