#include <unistd.h> /* execv(2), */
#include <stdlib.h> /* exit(3), getenv(3), setenv(3), */
#include <string.h> /* strcmp(3), */

int main(int argc, char *argv[])
{
	char *void_array[] = { NULL };

	if (getenv("PROC_SELF_EXE") != NULL)
		exit(EXIT_SUCCESS);

	setenv("PROC_SELF_EXE", "1", 1);
	execv("/proc/self/exe", void_array);
	exit(EXIT_FAILURE);
}
