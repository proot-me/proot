#include <unistd.h> /* execl(2), */
#include <stdio.h>  /* perror(3), */
#include <stdlib.h> /* exit(3), */

int main(void)
{
	int status;

	status = execl("/bin/ls", "ls", NULL);
	return 0;
}
