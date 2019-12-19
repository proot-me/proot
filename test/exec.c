#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[], char *envp[])
{
	if (argc < 2) {
		puts("not enough parameter: filename argv[0] argv[1] ... argv[n]");
		exit(EXIT_FAILURE);
	}

	execve(argv[1], &argv[2], envp);
	perror("execve");

	exit(EXIT_FAILURE);
}
