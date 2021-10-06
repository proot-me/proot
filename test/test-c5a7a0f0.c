#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

static void *setuid_124(void *unused)
{
	int status;

	status = setuid(124);
	if (status < 0) {
		perror("setuid");
		pthread_exit((void *)-1);
	}

	if (getuid() != 124) {
		perror("getuid");
		pthread_exit((void *)-1);
	}

	pthread_exit(NULL);
}

static void pterror(const char *message, int error)
{
	fprintf(stderr, "%s: %s\n", message, strerror(error));
}

int main(void)
{
	pthread_t thread;
	int child_status;
	void *result;
	int status;

	switch(fork()) {
	case -1:
		perror("fork");
		exit(EXIT_FAILURE);

	case 0: /* child */
		status = setuid(123);
		if (status < 0) {
			perror("setuid");
			exit(EXIT_FAILURE);
		}

		if (getuid() != 123) {
			perror("getuid");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);

	default: /* parent */
		break;
	}

	status = wait(&child_status);
	if (status < 0 || child_status != 0) {
		perror("wait()");
		exit(EXIT_FAILURE);
	}

	if (getuid() != 0) {
		fprintf(stderr, "getuid() == %d != 0\n", getuid());
		exit(EXIT_FAILURE);
	}

	status = pthread_create(&thread, NULL, setuid_124, NULL);
	if (status != 0) {
		pterror("pthread_create", status);
		exit(EXIT_FAILURE);
	}

	status = pthread_join(thread, &result);
	if (status != 0) {
		pterror("pthread_create", status);
		exit(EXIT_FAILURE);
	}

	if (result != NULL) {
		fprintf(stderr, "result != NULL\n");
		exit(EXIT_FAILURE);
	}

	if (getuid() != 124) {
		fprintf(stderr, "getuid() == %d != 124\n", getuid());
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
