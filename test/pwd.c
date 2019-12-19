#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_getcwd, */


int main(void)
{
	char path[PATH_MAX];
	int status;

	status = syscall(SYS_getcwd, path, PATH_MAX);
	if (status < 0) {
		perror("getcwd()");
		exit(EXIT_FAILURE);
	}

	puts(path);

	exit(EXIT_SUCCESS);
}
