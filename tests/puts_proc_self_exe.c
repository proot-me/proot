#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	char path[PATH_MAX];
	ssize_t status;

	status = readlink("/proc/self/exe", path, PATH_MAX - 1);
	if (status < 0 || status >= PATH_MAX)
		exit(EXIT_FAILURE);
	path[status] = '\0';

	puts(path);
	exit(EXIT_SUCCESS);
}
