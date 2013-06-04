#include <unistd.h> /* syscall(2), fork(2), usleep(3), */
#include <stdio.h>  /* perror(3), printf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_readlink, SYS_getcwd, */
#include <errno.h> /* errno, */

int main(void)
{
	pid_t pid;
	int status;
	int i;

	for (i = 0; i < 1000; i++) {
		pid = fork();
		switch (pid) {
		case -1:
			/* Is the maximum number of processes
			 * reached?  */
			if (errno == EAGAIN)
				break;
			perror("fork()");
			exit(EXIT_FAILURE);

		case 0: /* child */
			exit(EXIT_SUCCESS);

		default: /* parent */
			break;
		}
	}

	exit(EXIT_SUCCESS);
}
