#include <stdio.h> /* perror(3) */
#include <stdlib.h> /* exit(3), */
#include <sys/wait.h> /* wait(3) */
#include <unistd.h> /* fork(2), */

int main(void)
{
	int child_status;
	int status;

	switch (fork()) {
	case -1:
		exit(EXIT_FAILURE);

	case 0: /* child */
		return 13;

	default: /* parent */
		status = wait(&child_status);
		if (status < 0) {
			perror("wait()");
			exit(EXIT_FAILURE);
		}

		if (!WIFEXITED(child_status))
			exit(EXIT_FAILURE);

		if (WEXITSTATUS(child_status) != 13)
			exit(EXIT_FAILURE);

		exit(EXIT_SUCCESS);
	}
}
