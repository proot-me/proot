#include <unistd.h> /* execlp(2), */
#include <stdlib.h> /* exit(3), getenv(3), setenv(3)*/
#include <string.h> /* strcmp(3), */

int main(int argc, char *argv[])
{
	if (getenv("PROC_SELF_EXE") != NULL)
		exit(EXIT_SUCCESS);

	setenv("PROC_SELF_EXE", "1", 1);
	execlp("/proc/self/exe", NULL);
	exit(EXIT_FAILURE);
}
