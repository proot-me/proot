#include <unistd.h> /* execv(3), */
#include <stdio.h>  /* fprintf(3), */
#include <string.h> /* strcmp(3), */

int main(int argc, char *argv[])
{
	int i;

	fprintf(stderr, "proot-exec:");
	for (i = 0; i < argc; i++)
		fprintf(stderr, " %s", argv[i]);
	fprintf(stderr, "\n");

	execv(argv[0], argv);

	fprintf(stderr, "proot-exec: execv(\"%s\"): ", argv[0]);
	perror(NULL);
	return 1;
}
