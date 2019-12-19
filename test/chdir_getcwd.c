#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

int main(int argc, char *argv[])
{
	char path[PATH_MAX];
	int status;
	int fd;

	if (argc < 2) {
		fprintf(stderr, "missing argument\n");
		exit(EXIT_FAILURE);
	}

	status = chdir(argv[1]);
	if (status < 0) {
		perror("chdir()");
		exit(EXIT_FAILURE);
	}

	if (getcwd(path, PATH_MAX) == NULL) {
		perror("getcwd()");
		exit(EXIT_FAILURE);
	}

	printf("%s\n", get_current_dir_name());
	exit(EXIT_SUCCESS);
}
