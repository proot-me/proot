#include <unistd.h> /* syscall(2), fork(2), usleep(3), */
#include <stdio.h>  /* perror(3), printf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_readlink, SYS_getcwd, */

int main(void)
{
	pid_t pid;
	int status;
	char path[PATH_MAX];

	pid = fork();
	switch (pid) {
	case -1:
		perror("fork()");
		exit(EXIT_FAILURE);

	case 0: /* child */
		status = syscall(SYS_readlink, "/proc/self/cwd", path, PATH_MAX);
		if (status < 0) {
			perror("readlink()");
			exit(EXIT_FAILURE);
		}
		path[status] = '\0';
		printf("child: %s =", path);
		break;

	default: /* parent */
		usleep(500000);

		status = syscall(SYS_getcwd, path, PATH_MAX);
		if (status < 0) {
			perror("getcwd()");
			exit(EXIT_FAILURE);
		}
		printf("= parent: %s\n", path);
		break;
	}

	exit(EXIT_SUCCESS);
}
