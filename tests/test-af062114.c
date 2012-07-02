#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

int main(void)
{
	int fd;
	int status;
	bool stop = false;

	fd = open("/proc/self/cmdline", O_RDONLY);
	if (fd < 0) {
		perror("open()");
		exit(EXIT_FAILURE);
	}

	do {
		char buffer;
		status = read(fd, &buffer, 1);
		if (status < 0) {
			perror("read()");
			exit(EXIT_FAILURE);
		}

		stop = (status == 0);

		status = write(1, &buffer, 1);
		if (status < 0) {
			perror("write()");
			exit(EXIT_FAILURE);
		}
	} while (!stop);

	exit(EXIT_SUCCESS);
}
