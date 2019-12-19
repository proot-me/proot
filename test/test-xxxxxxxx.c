#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

extern char *environ[];

#define CONTENT "this isn't an executable"

int main(void)
{
	char * const argv[] = { "argv0", "argv1", "argv2", NULL };
	char tmp_name[] = "/tmp/proot-XXXXXX";
	int status;
	int fd;

	status = execve("/tmp", argv, environ);
	if (errno != EACCES) {
		perror("execve (1)");
		exit(EXIT_FAILURE);
	}

	fd = mkstemp(tmp_name);
	if (fd < 0) {
		perror("mkstemp");
		exit(EXIT_FAILURE);
	}

	status = write(fd, CONTENT, sizeof(CONTENT));
	if (status != sizeof(CONTENT)) {
		perror("write");
		exit(EXIT_FAILURE);
	}
	close(fd);

	status = chmod(tmp_name, 0700);
	if (status < 0)  {
		perror("chmod");
		exit(EXIT_FAILURE);
	}

	status = execve(tmp_name, argv, environ);
	if (errno != ENOEXEC) {
		perror("execve (2)");
		exit(EXIT_FAILURE);
	}

	printf("Check the stack integrity: %F + %F\n", (double) status, (double) errno);

	exit(EXIT_SUCCESS);
}
