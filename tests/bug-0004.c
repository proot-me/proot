#include <unistd.h> /* syscall(2), */
#include <stdio.h>  /* perror(3), fprintf(3), */
#include <limits.h> /* PATH_MAX, */
#include <stdlib.h> /* exit(3), */
#include <string.h> /* strlen(3), */
#include <sys/syscall.h> /* SYS_readlink, SYS_getcwd, */

int main(void)
{
	char path[PATH_MAX];
	int status;

	memset(path, -1, PATH_MAX * sizeof(char));

	status = syscall(SYS_readlink, "/proc/self/cwd", path, PATH_MAX);
	if (status < 0) {
		perror("readlink()");
		exit(EXIT_FAILURE);
	}
	path[status] = '\0';

	printf("->%s<-\n", path);
	return 0;
}
