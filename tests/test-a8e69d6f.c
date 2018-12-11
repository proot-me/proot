#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <stdlib.h> /* exit(3), */
#include <sys/syscall.h> /* SYS_lstat, */
#include <sys/stat.h>  /* struct stat, */
#include <fcntl.h> /* AT_FDCWD */

int main(void)
{
	struct stat stat;
	int status;

#if defined(SYS_lstat)
	status = syscall(SYS_lstat, "/proc/self/cwd/", &stat);
#elif defined(SYS_newfstatat)
    status = syscall(SYS_newfstatat, AT_FDCWD, "/proc/self/cwd/", &stat, 0);
#else
#error "SYS_lstat and SYS_newfstatat doesn't exists"
#endif
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
