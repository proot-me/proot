#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>

static void *routine(void *path)
{
	int status;

	status = chdir(path);
	if (status < 0) {
		perror("chdir");
		pthread_exit((void *)-1);
	}
	pthread_exit(NULL);
}

static void pterror(const char *message, int error)
{
	fprintf(stderr, "%s: %s\n", message, strerror(error));
}

void check_cwd(const char *expected)
{
	char path[PATH_MAX];
	int status;

	if (getcwd(path, PATH_MAX) == NULL) {
		perror("getcwd");
		exit(EXIT_FAILURE);
	}

	if (strcmp(path, expected) != 0) {
		fprintf(stderr, "getcwd: %s != %s\n", path, expected);
		exit(EXIT_FAILURE);
	}

	status = readlink("/proc/self/cwd", path, PATH_MAX - 1);
	if (status < 0) {
		perror("readlink");
		exit(EXIT_FAILURE);
	}
	path[status] = '\0';

	if (strcmp(path, expected) != 0) {
		fprintf(stderr, "readlink /proc/self/cwd: %s != %s\n", path, expected);
		exit(EXIT_FAILURE);
	}

}

int main(int argc, char *argv[])
{
	pthread_t thread;
	int child_status;
	void *result;
	int status;

	status = pthread_create(&thread, NULL, routine, "/etc");
	if (status != 0) {
		pterror("pthread_create", status);
		exit(EXIT_FAILURE);
	}

	status = pthread_join(thread, &result);
	if (status != 0) {
		pterror("pthread_create", status);
		exit(EXIT_FAILURE);
	}

	if (result != NULL)
		exit(EXIT_FAILURE);

	check_cwd("/etc");

	switch (fork()) {
	case -1:
		perror("readlink");
		exit(EXIT_FAILURE);

	case 0: /* child */
		status = chdir("/etc");
		if (status < 0) {
			perror("chdir");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);

	default:
		status = wait(&child_status);
		if (status < 0 || child_status != 0) {
			perror("wait()");
			exit(EXIT_FAILURE);
		}
		check_cwd("/etc");
		break;
	}

	exit(EXIT_SUCCESS);
}

