#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

int main()
{
	int fd;

	fd = open("/bin/true", O_RDONLY);
	if (fd < 0) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);

	fd = open("/bin/true/", O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);

	fd = open("/bin/true/.", O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);

	fd = open("/bin/true/..", O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);

	fd = open("/6a05942f08d5a72de56483487963deec", O_RDONLY);
	if (fd >= 0 || errno != ENOENT) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);

	fd = open("/6a05942f08d5a72de56483487963deec/", O_RDONLY);
	if (fd >= 0 || errno != ENOENT) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);

	fd = open("/6a05942f08d5a72de56483487963deec/.", O_RDONLY);
	if (fd >= 0 || errno != ENOENT) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);

#if 0
	/* This test fails in OBS, why?  */
	fd = open("/6a05942f08d5a72de56483487963deec/..", O_RDONLY);
	if (fd >= 0 || errno != ENOENT) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(fd);
#endif

	exit(EXIT_SUCCESS);
}
