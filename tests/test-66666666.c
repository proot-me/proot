#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

bool sigtrap_received = false;

void handler(int signo)
{
	if (signo == SIGTRAP)
		sigtrap_received = true;
}

int main()
{
	struct sigaction sa;
	int status;

	sa.sa_flags = 0;
	sa.sa_handler = handler;
	status = sigemptyset(&sa.sa_mask);
	if (status < 0) {
		perror("sigemptyset()");
		exit(EXIT_FAILURE);
	}

	status = sigaction(SIGTRAP, &sa, 0);
	if (status < 0) {
		perror("sigaction(SIGTRAP)");
		exit(EXIT_FAILURE);
	}

	status = raise(SIGTRAP);
	if (status != 0) {
		perror("raise(SIGTRAP)");
		exit(EXIT_FAILURE);
	}

	if (sigtrap_received)
		exit(EXIT_SUCCESS);
	else
		exit(EXIT_FAILURE);
}
