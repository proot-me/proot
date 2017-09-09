#include <unistd.h>     /* fork(2), */
#include <stdio.h>      /* perror(3), fprintf(3), */
#include <stdlib.h>     /* exit(3), */
#include <sys/ptrace.h> /* ptrace(2), */
#include <sys/types.h>  /* waitpid(2), */
#include <sys/wait.h>   /* waitpid(2), */

int main(void)
{
	int child_status, status;
	pid_t pid;

	pid = fork();
	switch (pid) {
	case -1:
		perror("fork()");
		exit(EXIT_FAILURE);

	case 0: /* child */
		status = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (status < 0) {
			perror("ptrace(TRACEME)");
			exit(EXIT_FAILURE);
		}

		exit(EXIT_SUCCESS);

	default: /* parent */
		status = waitpid(pid, &child_status, 0);
		if (status < 0) {
			perror("waitpid()");
			exit(EXIT_FAILURE);
		}

		if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
			perror("unexpected child status\n");
			exit(EXIT_FAILURE);
		}

		exit(EXIT_SUCCESS);
	}

	/* Unreachable. */
	exit(EXIT_FAILURE);
}
