#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main()
{
	int status;
	char *path;

	path = tmpnam(NULL);
	status = symlink(path, path);
	if (status < 0)
		exit(EXIT_FAILURE);

	status = fchownat(AT_FDCWD, path, getuid(), getgid(), 0);
	if (status >= 0)
		exit(EXIT_FAILURE);

	status = fchownat(AT_FDCWD, path, getuid(), getgid(), AT_SYMLINK_NOFOLLOW);
	if (status < 0)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
