#include <stdlib.h> /* atoi(3), NULL, exit(3), EXIT_*, */
#include <stdio.h>  /* printf(3), perror(3), */
#include <limits.h> /* PATH_MAX, */

extern int proot(char result[PATH_MAX], const char *new_root, const char *fake_path, int deref_final);

int main(int argc, char *argv[])
{
	char result[PATH_MAX];
	int status = 0;

	if (argc != 4) {
		printf("usage: proot <new_root> <fake_path> <deref_final>\n");
		return EXIT_FAILURE;
	}

	status = proot(result, argv[1], argv[2], atoi(argv[3]));
	if (status != 0) {
		perror("proot");
		return EXIT_FAILURE;
	}

	printf("proot: %s\n", result);

	return EXIT_SUCCESS;
}
