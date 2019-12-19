#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	struct dirent *dirents;
	DIR *dir;

	int status;
	int i;

	for (i = 1; i < argc; i++) {
		dir = opendir(argv[i]);
		if (dir == NULL) {
			perror("opendir(3)");
			exit(EXIT_FAILURE);
		}

		errno = 0;
		while ((dirents = readdir(dir)) != NULL) {
			printf("%s %s\n",
				dirents->d_type == DT_BLK  ? "DT_BLK " :
				dirents->d_type == DT_CHR  ? "DT_CHR " :
				dirents->d_type == DT_DIR  ? "DT_DIR " :
				dirents->d_type == DT_FIFO ? "DT_FIFO" :
				dirents->d_type == DT_LNK  ? "DT_LNK " :
				dirents->d_type == DT_REG  ? "DT_REG " :
				dirents->d_type == DT_SOCK ? "DT_SOCK" :
				"DT_UNKNOWN", dirents->d_name);
			errno = 0;
		}

		if (errno != 0) {
			perror("readdir(3)");
			exit(EXIT_FAILURE);
		}

		status = closedir(dir);
		if (status < 0) {
			perror("closedir(3)");
			exit(EXIT_FAILURE);
		}
	}

	exit(EXIT_SUCCESS);
}
