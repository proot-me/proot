/* Note: loop infinitely when -r is specified to PRoot (use of ELF
 * interpreter), because argc is never equal to 0.
 */

#include <unistd.h> /* execve(2), */
#include <stdlib.h> /* exit(3), */
#include <string.h> /* strcmp(3), */

int main(int argc, char *argv[])
{
	char *void_array[] = { NULL };

	if (argc == 0)
		exit(EXIT_SUCCESS);

	execve("/proc/self/exe", void_array, void_array);
	exit(EXIT_FAILURE);
}
