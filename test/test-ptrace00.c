/* Extracted from strace-4.8/strace.c:test_ptrace_setoptions_for_all.  */

#include <sys/ptrace.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	bool does_work = false;
	int status;
	pid_t pid;

	switch(pid = fork()) {
	case -1:
		perror("fork");
		exit(EXIT_FAILURE);

	case 0:
		status = ptrace(PTRACE_TRACEME, 0, 0, 0);
		if (status < 0) {
			perror("ptrace(PTRACE_TRACEME)");
			exit(EXIT_FAILURE);
		}

		kill(getpid(), SIGSTOP);
		exit(EXIT_SUCCESS);

	default:
		break;
	}

	while (1) {
		fprintf(stderr, ">>> pid = wait(&status)\n");
		pid = wait(&status);
		if (pid < 0) {
			perror("wait");
			exit(EXIT_FAILURE);
		}
		else if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) == 0) {
				fprintf(stderr, ">>> EXITSTATUS(status) == 0\n");
				break;
			}
			fprintf(stderr, "WEXITSTATUS != 0\n");
			exit(EXIT_FAILURE);
		}
		else if (WIFSIGNALED(status)) {
			fprintf(stderr, "WIFSIGNALED\n");
			exit(EXIT_FAILURE);
		}
		else if (!WIFSTOPPED(status)) {
			fprintf(stderr, "!WIFSTOPPED\n");
			exit(EXIT_FAILURE);
		}
		else if (WSTOPSIG(status) == SIGSTOP) {
			fprintf(stderr, ">>> ptrace(PTRACE_SETOPTIONS, ...)\n");
			status = ptrace(PTRACE_SETOPTIONS, pid, 0,
					PTRACE_O_TRACESYSGOOD |	PTRACE_O_TRACEEXEC);
			if (status < 0) {
				perror("ptrace(PTRACE_SETOPTIONS)");
				exit(EXIT_FAILURE);
			}
		}
		else if (WSTOPSIG(status) == (SIGTRAP | 0x80)) {
			fprintf(stderr, ">>> does_work = true\n");
			does_work = true;
		}

		fprintf(stderr, ">>> ptrace(PTRACE_SYSCALL, ...)\n");
		status = ptrace(PTRACE_SYSCALL, pid, 0, 0);
		if (status < 0) {
			perror("ptrace(PTRACE_SYSCALL)");
			exit(EXIT_FAILURE);
		}
	}

	fprintf(stderr, ">>> exit(...)\n");
	exit(does_work ? EXIT_SUCCESS : EXIT_FAILURE);
}
