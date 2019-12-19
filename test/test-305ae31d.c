#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main()
{
	int fd;
	int fd_dir;
	int fd_file;
	char path[64]; /* 64 > sizeof("/proc//fd/") + 2 * sizeof(#ULONG_MAX) */
	int status;

	fd_dir = open("/bin", O_RDONLY);
	if (fd_dir < 0)
		exit(EXIT_FAILURE);

	fd_file = open("/bin/true", O_RDONLY);
	if (fd_file < 0)
		exit(EXIT_FAILURE);

	status = snprintf(path, sizeof(path), "/proc/%d/fd/%d/", getpid(), fd_dir);
	if (status < 0 || status >= sizeof(path))
		exit(EXIT_FAILURE);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		exit(EXIT_FAILURE);
	close(fd);

	status = snprintf(path, sizeof(path), "/proc/%d/fd/%d/..", getpid(), fd_dir);
	if (status < 0 || status >= sizeof(path))
		exit(EXIT_FAILURE);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		exit(EXIT_FAILURE);
	close(fd);

	status = snprintf(path, sizeof(path), "/proc/%d/fd/%d/..", getpid(), fd_file);
	if (status < 0 || status >= sizeof(path))
		exit(EXIT_FAILURE);

	fd = open(path, O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR)
		exit(EXIT_FAILURE);

	status = snprintf(path, sizeof(path), "/proc/%d/fd/999999/..", getpid());
	if (status < 0 || status >= sizeof(path))
		exit(EXIT_FAILURE);

	fd = open(path, O_RDONLY);
	if (fd >= 0 || errno != ENOENT)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
