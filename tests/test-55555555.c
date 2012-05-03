#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	int status;

	status = signal(SIGTERM, SIG_IGN);
	if (status < 0) {
		perror("signal");
		exit(EXIT_FAILURE);
	}

	status = kill(0, SIGTERM);
	if (status < 0) {
		perror("kill");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
