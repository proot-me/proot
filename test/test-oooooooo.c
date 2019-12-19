#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	int i;

	for (i = 0; i < 999; i++) {
		int status;
		int fds[2];
		pid_t pid;

		status = pipe(fds);
		if (status < 0) {
			perror("pipe");
			break;
		}

		pid = fork();
		switch (pid) {
		case -1:
			perror("fork");
			exit(EXIT_FAILURE);

		case 0: /* child */
			do status = write(fds[1], "!", 1); while (status > 0);
			perror("write");
			exit(EXIT_FAILURE);

		default: /* parent */
			status = kill(pid, SIGKILL);
			if (status < 0) {
				perror("kill");
				exit(EXIT_FAILURE);
			}
		}
	}

	exit(EXIT_SUCCESS);
}
