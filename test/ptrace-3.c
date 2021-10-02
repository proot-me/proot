#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>      /* errno(3), */
#include <sys/user.h>   /* struct user*, */

int main(int argc, char **argv)
{
	enum __ptrace_request restart_how;
	int last_exit_status = -1;
	int child_status;
	pid_t *pids = NULL;
	long status;
	int signal;
	pid_t pid;

	if (argc <= 1) {
		fprintf(stderr, "Usage: %s /path/to/exe [args]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	pid = fork();
	switch(pid) {
	case -1:
		perror("fork()");
		exit(EXIT_FAILURE);

	case 0: /* child */
		status = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (status < 0) {
			perror("ptrace(TRACEME)");
			exit(EXIT_FAILURE);
		}

		/* Synchronize with the tracer's event loop.  */
		kill(getpid(), SIGSTOP);

		execvp(argv[1], &argv[1]);
		exit(EXIT_FAILURE);

	default: /* parent */
		break;
	}

	while (1) {
		int tracee_status;
		pid = waitpid(-1, &tracee_status, __WALL);
		if (pid < 0) {
			perror("waitpid()");
			if (errno != ECHILD)
				exit(EXIT_FAILURE);
			break;
		}

		if (WIFEXITED(tracee_status)) {
			fprintf(stderr, "pid %d: exited with status %d\n",
				pid, WEXITSTATUS(tracee_status));
			last_exit_status = WEXITSTATUS(tracee_status);
			continue; /* Skip the call to ptrace(SYSCALL). */
		}
		else if (WIFSTOPPED(tracee_status)) {
			int signal = tracee_status >> 8;
			if (signal != SIGTRAP && signal != SIGSTOP) {
				// We expect a SIGSTOP since the child sends it to itself
				// and a SIGTRAP from the exec + ptrace.
				// Anything else is an error.
				fprintf(stderr, "Unexpected signal recieved from pid: %d.\n",
					pid);
				exit(EXIT_FAILURE);
			}
			status = ptrace(PTRACE_CONT, pid, NULL, 0);
		}
	}

	return last_exit_status;
}
