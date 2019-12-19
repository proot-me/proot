#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	int status;
	int fd;
	int i;

	for (i = 1; i < argc; i++) {
		char buffer[1024];

		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			perror("open(2)");
			exit(EXIT_FAILURE);
		}

		while ((status = read(fd, buffer, sizeof(buffer))) > 0 && write(1, buffer, status) == status)
			errno = 0;

		if (errno != 0) {
			perror("read(2)/write(2)");
			exit(EXIT_FAILURE);
		}

		(void) close(fd);
	}

	exit(EXIT_SUCCESS);
}
