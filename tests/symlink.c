#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_symlink, */

int main(int argc, char *argv[])
{
	int status;

	if (argc != 3) {
		fprintf(stderr, "usage: symlink REFEREE REFERER\n");
		exit(EXIT_FAILURE);
	}

	status = syscall(SYS_symlink, argv[1], argv[2]);
	if (status < 0) {
		perror("symlink()");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
