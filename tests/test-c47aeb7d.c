#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void *print_hello(void *id)
{
	pthread_exit(id);
}

int main(void)
{
	const int nb_threads = 10;
	pthread_t threads[nb_threads];
	int status;
	long i;

	for(i = 0; i < nb_threads; i++) {
		status = pthread_create(&threads[i], NULL, print_hello, (void *) i);
		if (status != 0)
			exit(EXIT_FAILURE);
	}

	for(i = 0; i < nb_threads; i++) {
		intptr_t result;

		status = pthread_join(threads[i], (void **) &result);
		if (status != 0 || (int) result != i)
			exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
