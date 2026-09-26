#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif
#ifndef PR_GET_NO_NEW_PRIVS
#define PR_GET_NO_NEW_PRIVS 39
#endif

/* Print the no_new_privs flag as prctl(2) reports it.  With "set",
 * set it first, then print it from a child and from a new program
 * as well, since both inherit it.  */
int main(int argc, char *argv[])
{
    int status;

    if (argc > 1 && strcmp(argv[1], "set") == 0) {
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
	    return 1;
	printf("%d\n", prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0));
	fflush(stdout);

	if (fork() == 0) {
	    printf("%d\n", prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0));
	    return 0;
	}
	wait(&status);

	execl(argv[0], argv[0], (char *) NULL);
	return 1;
    }

    printf("%d\n", prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0));
    return 0;
}
