#include <unistd.h> /* execve(2), */
#include <stdlib.h> /* exit(3), */
#include <string.h> /* strcmp(3), */

int main(int argc, char *argv[])
{
	if (argc == 0)
		exit(EXIT_SUCCESS);

	execve("/proc/self/exe", NULL, NULL);
	exit(EXIT_FAILURE);
}
