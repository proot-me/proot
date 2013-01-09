#include <unistd.h> /* execlp(2), */
#include <stdlib.h> /* exit(3), */
#include <string.h> /* strcmp(3), */

int main(int argc, char *argv[])
{
	if (argc == 0) //strcmp(argv[0], "/proc/self/exe") == 0)
		exit(EXIT_SUCCESS);

	execlp("/proc/self/exe", NULL);
	exit(EXIT_FAILURE);
}
