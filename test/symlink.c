#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_symlink, */
#include <fcntl.h> /* AT_FDCWD */

int main(int argc, char *argv[])
{
	int status;

	if (argc != 3) {
		fprintf(stderr, "usage: symlink REFEREE REFERER\n");
		exit(EXIT_FAILURE);
	}

#if defined(SYS_symlink)
	status = syscall(SYS_symlink, argv[1], argv[2]);
#elif defined(SYS_symlinkat)
	status = syscall(SYS_symlinkat, argv[1], AT_FDCWD, argv[2]);
#else
#error "SYS_symlink and SYS_symlinkat doesn't exists"
#endif
	if (status < 0) {
		perror("symlink()");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
