#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 5

void *exec(void *id)
{
	char *const argv[] = { "true", NULL };

	if ((long) id == NUM_THREADS - 1)
		execve("/usr/bin/true", argv, NULL);
	else
		sleep(50);

	pthread_exit(NULL);
}

int main()
{
	pthread_t threads[NUM_THREADS];
	int status;
	long i;

	exit(125); /* NYI */

	for(i = 0; i < NUM_THREADS ; i++) {
		status = pthread_create(&threads[i], NULL, exec, (void *) i);
		if (status)
			exit(EXIT_FAILURE);
		sleep(1);
	}

	sleep(50);
	exit(EXIT_FAILURE);
}
