#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_lstat, */
#include <sys/stat.h>  /* struct stat, */

int main(void)
{
	struct stat stat;
	int status;

	status = syscall(SYS_lstat, "/proc/self/cwd/", &stat);
	if (status < 0) {
		perror("lstat()");
		exit(EXIT_FAILURE);
	}

	if (S_ISLNK(stat.st_mode)) {
		fprintf(stderr, "trailing '/' ignored\n");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
