#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <strings.h> /* bzero(3), */
#include <sys/syscall.h> /* SYS_readlink, */

int main(int argc, char *argv[])
{
	char path[PATH_MAX];
	int status;

	bzero(path, sizeof(path));

	status = syscall(SYS_readlink, "/proc/self/exe", path, PATH_MAX);
	if (status < 0) {
		perror("readlink()");
		exit(EXIT_FAILURE);
	}

	if (status >= PATH_MAX)
		return 125;

	if (path[status] != '\0') {
		path[PATH_MAX - 1] = '\0';
		puts(path);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
