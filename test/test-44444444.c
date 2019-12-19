#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	char buffer[2 * PATH_MAX];

	if (!getcwd(buffer, sizeof(buffer))) {
		perror("getcwd");
		exit(EXIT_FAILURE);
	}

	if (readlink("/bin/abs-true", buffer, sizeof(buffer)) < 0) {
		perror("readlink");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
