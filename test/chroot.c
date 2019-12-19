#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc < 2)
		exit(1);

	if (chroot(argv[1]))
		exit(1);

	if (argc >= 3)
		execvp(argv[2], &argv[2]);
	else
		execlp("sh", "sh", NULL);

	/* unreachable */
	return 1;
}
