#include <stdlib.h> /* exit(3), */
#include <unistd.h> /* fork(2), sleep(3), */
#include <sys/types.h> /* wait(2), */
#include <sys/wait.h>  /* wait(2), */

int main(int argc, char *argv[])
{
	int child_status;
	int status;

	switch (fork()) {
	case -1:
		exit(EXIT_FAILURE);

	case 0: /* Child */
		argc > 1 && sleep(2);
		exit(EXIT_SUCCESS);

	default: /* Parent */
		argc <= 1 && sleep(2);
		status = wait(&child_status);
		if (status < 0 || !WIFEXITED(child_status))
			exit(EXIT_FAILURE);
		exit(WEXITSTATUS(child_status));
	}
}
