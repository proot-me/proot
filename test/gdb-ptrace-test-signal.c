//

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/compat.h"

static int fork_to_function(int (*function)(void))
{
	int child_pid = fork();

	if (child_pid == -1) {
		perror("fork()");
		exit(EXIT_FAILURE);
	}

	if (child_pid == 0)
		return function();

	return child_pid;
}

static int grandchild_function(void)
{
	exit(0);
}

static int child_function(void)
{
	ptrace(PTRACE_TRACEME, 0, 0, 0);
	kill(getpid(), SIGSTOP);

	fork_to_function(grandchild_function);

	exit(0);
}

static void kill_child(int child_pid)
{
	pid_t got_pid;
	int kill_status;

	if (kill(child_pid, SIGKILL) != 0) {
		perror("kill()");
		exit(EXIT_FAILURE);
	}

	got_pid = waitpid (child_pid, &kill_status, 0);
	if (got_pid != child_pid) {
		fprintf(stderr, "waitpid: unexpected result %d.", (int)got_pid);
		exit(EXIT_FAILURE);
	}

	if (!WIFSIGNALED(kill_status)) {
		fprintf(stderr, "waitpid: unexpected status %d.", kill_status);
		exit(EXIT_FAILURE);
	}
}

static void test_tracefork(int child_pid)
{
	int ret, status;
	long second_pid;

	ret = ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_TRACEFORK);
	if (ret != 0) {
		// Skipped
		perror("ptrace(PTRACE_SETOPTIONS, PTRACE_O_TRACEFORK)");
		kill_child(child_pid);
		exit(125);
	}

	ret = ptrace (PTRACE_CONT, child_pid, 0, 0);
	if (ret != 0) {
		perror("ptrace(PTRACE_CONT)");
		kill_child(child_pid);
		exit(EXIT_FAILURE);
	}

	ret = waitpid(child_pid, &status, 0);
	/* Check if we received a fork event notification.  */
	if (ret == child_pid && WIFSTOPPED(status) && (status >> 16) == PTRACE_EVENT_FORK) {
		/* We did receive a fork event notification.  Make sure its PID
		   is reported.  */
		second_pid = 0;
		ret = ptrace(PTRACE_GETEVENTMSG, child_pid, 0, &second_pid);
		if (ret == 0 && second_pid != 0) {
			int second_status;

			/* Do some cleanup and kill the grandchild.  */
			waitpid(second_pid, &second_status, 0);
			kill_child(second_pid);
		}
		else {
			perror("ptrace(PTRACE_GETEVENTMSG)");
			exit(EXIT_FAILURE);
		}
	}
	else {
		fprintf(stderr, "Unexpected result from waitpid: pid=%d, status=%d\n", ret, status);
		exit(EXIT_FAILURE);
	}
}

static void check_ptrace_features(int do_fork)
{
	int child_pid, ret, status;

	child_pid = fork_to_function(child_function);

	ret = waitpid(child_pid, &status, 0);
	if (ret == -1) {
		perror("waitpid()");
		exit(EXIT_FAILURE);
	}
	else if (ret != child_pid) {
		fprintf(stderr, "waitpid: unexpected result %d.", ret);
		exit(EXIT_FAILURE);
	}
	else if (!WIFSTOPPED(status)) {
		fprintf(stderr, "waitpid: unexpected status %d.", status);
		exit(EXIT_FAILURE);
	}

	if (do_fork)
		test_tracefork(child_pid);

	kill_child(child_pid);
}

static int devnull = -1;

// SIGCHLD handler which has a high chance of triggering
// right before the chained wait4 call.
// We need to make sure things are handled correctly even if we
static void sigchld_handler(int signo)
{
	if (devnull != -1)
		close(devnull);
	devnull = open("/dev/null", O_RDONLY);
	if (devnull == -1) {
		perror("open(/dev/null)");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	struct sigaction sigchld_action = {};
	sigchld_action.sa_handler = sigchld_handler;
	sigemptyset (&sigchld_action.sa_mask);
	sigchld_action.sa_flags = SA_RESTART;

	/* Make it the default.  */
	sigaction(SIGCHLD, &sigchld_action, NULL);
	(void)argv;
	check_ptrace_features(argc > 1);
	if (devnull == -1) {
		fprintf(stderr, "SIGCHLD handler not called.\n");
		exit(EXIT_FAILURE);
	}
	close(devnull);
	return 0;
}
