#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

void *routine(void *path)
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

int main(int argc, char *argv[])
{
	int status;
	void *result;
	pthread_t thread;
	char path[PATH_MAX];

	status = pthread_create(&thread, NULL, routine, "/bin");
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

	if (getcwd(path, PATH_MAX) == NULL) {
		perror("getcwd");
		exit(EXIT_FAILURE);
	}

	if (strcmp(path, "/bin") != 0) {
		fprintf(stderr, "getcwd: %s != %s\n", path, "/bin");
		exit(EXIT_FAILURE);
	}

	status = readlink("/proc/self/cwd", path, PATH_MAX - 1);
	if (status < 0) {
		perror("readlink");
		exit(EXIT_FAILURE);
	}
	path[status] = '\0';

	if (strcmp(path, "/bin") != 0) {
		fprintf(stderr, "readlink /proc/self/cwd: %s != %s\n", path, "/bin");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

