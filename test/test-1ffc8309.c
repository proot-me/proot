#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

int main()
{
	int fds[2];
	int status;
	uint8_t buffer;

	status = pipe2(fds, O_NONBLOCK);
	if (status < 0) {
		perror("pipe2");
		exit(EXIT_FAILURE);
	}

	(void) alarm(5);

	(void) read(fds[0], &buffer, 1);
	if (errno != EAGAIN && errno != EWOULDBLOCK) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
