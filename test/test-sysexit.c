/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*- */
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/wait.h>  /* wait(2), */
#include <unistd.h>
#include <string.h>

/* Related to github issue #106.
 * Test if sysexit handlers execute, using uname handling
 * in the kompat extension. The test case is meant to be
 * run with "-k 3.4242XX" cmdline argument.
 * The bug could occur during the first traced syscall,
 * before seccomp got enabled.
 * However there was some kind of a random factor there,
 * so we fork out many processes to make it likely that at least
 * some would hit this problem. */

int main()
{
	int status;
	struct utsname s;
	for (int i = 0; i < 5; i++) {
		fork();
	}
	uname(&s);
	int child_status;
	while ((status = wait(&child_status)) >= 0) {
		if (!WIFEXITED(child_status) || (WEXITSTATUS(child_status) == EXIT_FAILURE))
			exit(EXIT_FAILURE);
	}
	if (strcmp("3.4242XX", s.release) == 0) {
		exit(EXIT_SUCCESS);
	}
	exit(EXIT_FAILURE);
}
