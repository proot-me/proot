#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

void handler(int signo)
{
	if (signo == SIGSTOP)
		exit(EXIT_SUCCESS);
}

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
		exit(EXIT_FAILURE);

	default: /* parent */
		status = waitpid(pid, &child_status, WUNTRACED);
		if (status < 0) {
			perror("waitpid()");
			exit(EXIT_FAILURE);
		}
		if (WIFSTOPPED(child_status))
			exit(EXIT_SUCCESS);
		else
			exit(EXIT_FAILURE);
	}
}

