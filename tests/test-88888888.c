#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

int main()
{
	int fd;

	fd = open("/bin/true", O_RDONLY);
	if (fd < 0)
		exit(EXIT_FAILURE);
	close(fd);

	fd = open("/bin/true/", O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR)
		exit(EXIT_FAILURE);
	close(fd);

	fd = open("/bin/true/.", O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR)
		exit(EXIT_FAILURE);
	close(fd);

	fd = open("/bin/true/..", O_RDONLY);
	if (fd >= 0 || errno != ENOTDIR)
		exit(EXIT_FAILURE);
	close(fd);

	fd = open("/6a05942f08d5a72de56483487963deec", O_RDONLY);
	if (fd >= 0 || errno != ENOENT)
		exit(EXIT_FAILURE);
	close(fd);

	fd = open("/6a05942f08d5a72de56483487963deec/", O_RDONLY);
	if (fd >= 0 || errno != ENOENT)
		exit(EXIT_FAILURE);
	close(fd);

	fd = open("/6a05942f08d5a72de56483487963deec/.", O_RDONLY);
	if (fd >= 0 || errno != ENOENT)
		exit(EXIT_FAILURE);
	close(fd);

	fd = open("/6a05942f08d5a72de56483487963deec/..", O_RDONLY);
	if (fd >= 0 || errno != ENOENT)
		exit(EXIT_FAILURE);
	close(fd);

	exit(EXIT_SUCCESS);
}
