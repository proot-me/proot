#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
	int child_status;
	long status;
	pid_t pid;

	pid = (argc <= 1 ? fork() : vfork());
	switch(pid) {
	case -1:
		perror("fork()");
		exit(EXIT_FAILURE);

	case 0: /* child */
		sleep(2);
		status = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (status < 0) {
			perror("ptrace(TRACEME)");
			exit(EXIT_FAILURE);
		}

		if (argc <= 1) {
			kill(getpid(), SIGSTOP);
			exit(EXIT_SUCCESS);
		}
		else {
			execl("true", "true", NULL);
			exit(EXIT_FAILURE);
		}

	default: /* parent */
		pid = waitpid(-1, &child_status, __WALL);
		if (pid < 0) {
			perror("waitpid()");
			exit(EXIT_FAILURE);
		}

		if (argc <= 1) {
			if (!WIFSTOPPED(child_status))
				exit(EXIT_FAILURE);
		}

		exit(EXIT_SUCCESS);
	}

	return 0;
}
