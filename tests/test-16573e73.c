#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **ignored)
{
	char *const argv[] = { "true", NULL};
	char *const envp[] = { NULL };

	pid_t pid;
	int status;

	pid = (argc <= 1 ? vfork() : fork());
	switch (pid) {
	case -1:
		exit(EXIT_FAILURE);

	case 0: /* child */
		exit(execve("/bin/true", argv, envp));

	default: /* parent */
		if (wait(&status) < 0 || !WIFEXITED(status))
			exit(EXIT_FAILURE);
		exit(WEXITSTATUS(status));
	}

	exit(EXIT_FAILURE);
}
