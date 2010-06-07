#include <stdlib.h> /* atoi(3), NULL, exit(3), EXIT_*, */
#include <stdio.h>  /* printf(3), perror(3), */

extern char *proot(const char *new_root, const char *fake_path, int deref_final);

int main(int argc, char *argv[])
{
	char *result;

	if (argc != 4) {
		printf("usage: proot <new_root> <fake_path> <deref_final>\n");
		return EXIT_FAILURE;
	}

	result = proot(argv[1], argv[2], atoi(argv[3]));
	if (result == NULL) {
		perror("proot");
		return EXIT_FAILURE;
	}

	printf("result: %s\n", result);
	free(result);

	return EXIT_SUCCESS;
}
