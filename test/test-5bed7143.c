#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define TEMPLATE "/tmp/proot-test-5bed7143-XXXXXX"
#define COOKIE1 "2fde3df3558fa30bec1b8ebad42df20f"
#define COOKIE2 "2ba90289e48d1896e0601239ac25f764"

int main()
{
	char *path1;
	char *path2;
	char *cwd;
	int status;

	path1 = mkdtemp(strdup(TEMPLATE));
	if (path1 == NULL)
		exit(EXIT_FAILURE);

	status = chdir(path1);
	if (status < 0)
		exit(EXIT_FAILURE);

	status = mkdir(COOKIE1, 0777);
	if (status < 0)
		exit(EXIT_FAILURE);

	status = chdir(COOKIE1);
	if (status < 0)
		exit(EXIT_FAILURE);

	status = creat(COOKIE2, O_RDWR);
	if (status < 0)
		exit(EXIT_FAILURE);
	close(status);

	path2 = mktemp(strdup(TEMPLATE));
	status = rename(path1, path2);
	if (status < 0)
		exit(EXIT_FAILURE);

	status = access(COOKIE2, F_OK);
	if (status < 0)
		exit(EXIT_FAILURE);

	cwd = get_current_dir_name();
	if (cwd == NULL || memcmp(cwd, path2, strlen(path2)) != 0)
		exit(EXIT_FAILURE);

	status = unlink(COOKIE2);
	if (status < 0)
		exit(EXIT_FAILURE);

	status = rmdir(cwd);
	if (status < 0)
		exit(EXIT_FAILURE);

	if (get_current_dir_name() != NULL || errno != ENOENT)
		exit(EXIT_FAILURE);

	status = rmdir(path2);
	if (status < 0)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
