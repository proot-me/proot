#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{
	int child_status;
	int status;
	pid_t pid;

	pid = fork();
	switch (pid) {
	case -1:
		perror("fork()");
		exit(EXIT_FAILURE);

	case 0: /* child */
		status = raise(SIGSTOP);
		if (status != 0) {
			perror("raise(SIGSTOP)");
			exit(EXIT_FAILURE);
		}
		sleep(1);
		exit(EXIT_FAILURE);

	default: /* parent */
		status = waitpid(pid, &child_status, WUNTRACED);
		if (status < 0) {
			perror("waitpid()");
			exit(EXIT_FAILURE);
		}

		if (WIFEXITED(child_status))
			printf("exited, status=%d\n", WEXITSTATUS(child_status));
		else if (WIFSIGNALED(child_status))
			printf("killed by signal %d\n", WTERMSIG(child_status));
		else if (WIFSTOPPED(child_status))
			printf("stopped by signal %d\n", WSTOPSIG(child_status));
		else if (WIFCONTINUED(child_status))
			printf("continued\n");

		if (WIFSTOPPED(child_status))
			exit(EXIT_SUCCESS);
		else
			exit(EXIT_FAILURE);
	}
}

