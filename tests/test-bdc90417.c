#define _GNU_SOURCE       /* See feature_test_macros(7) */
#include <unistd.h>       /* execv(3), syscall(2), */
#include <sys/syscall.h>  /* SYS_*, */
#include <sys/time.h>     /* *rlimit(2), */
#include <sys/resource.h> /* *rlimit(2), */
#include <stdlib.h>       /* EXIT_*, exit(3), */

int main(int argc, char *argv[])
{
	char *const dummy_argv[] = { "test", "stage2", NULL };
	long brk1, brk2;
	int status;
	struct rlimit rlimit;

	switch (argc) {
	case 1: /* 1st step: set the stack limit to the max.  */
		status = getrlimit(RLIMIT_STACK, &rlimit);
		if (status < 0)
			exit(EXIT_FAILURE);

		rlimit.rlim_cur = rlimit.rlim_max;

		status = setrlimit(RLIMIT_STACK, &rlimit);
		if (status < 0)
			exit(EXIT_FAILURE);

		return execv(argv[0], dummy_argv);

	default: /* 2nd step: try to allocate some heap space.  */
		brk1 = syscall(SYS_brk, 0);
		brk2 = syscall(SYS_brk, brk1 + 1024 * 1024);
		exit(brk1 != brk2 ? EXIT_SUCCESS : EXIT_FAILURE);
	}
}
