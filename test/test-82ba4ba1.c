#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	uid_t ruid = 13, euid = 13, suid = 13;
	gid_t rgid = 13, egid = 13, sgid = 13;
	int status;

	status = getresuid(&ruid, &euid, &suid);
	if (status != 0 || ruid != 0 || euid != 0 || suid != 0) {
		perror("getresuid");
		fprintf(stderr, "%ld %ld %ld\n", (unsigned long) ruid, (unsigned long) euid, (unsigned long) suid);
		exit(EXIT_FAILURE);
	}

	status = getresgid(&rgid, &egid, &sgid);
	if (status != 0 || rgid != 0 || egid != 0 || sgid != 0) {
		perror("getresgid");
		fprintf(stderr, "%ld %ld %ld\n", (unsigned long) ruid, (unsigned long) euid, (unsigned long) suid);
		exit(EXIT_FAILURE);
	}

	status = setresgid(1, 1, 1);
	if (status != 0) {
		perror("setresgid");
		exit(EXIT_FAILURE);
	}

	status = getresgid(&rgid, &egid, &sgid);
	if (status != 0 || rgid != 1 || egid != 1 || sgid != 1) {
		perror("getresgid");
		fprintf(stderr, "%ld %ld %ld\n", (unsigned long) rgid, (unsigned long) egid, (unsigned long) sgid);
		exit(EXIT_FAILURE);
	}

	if (status != 0 || rgid != 1 || egid != 1 || sgid != 1) {
		perror("getresgid");
		fprintf(stderr, "%ld %ld %ld\n", (unsigned long) ruid, (unsigned long) euid, (unsigned long) suid);
		exit(EXIT_FAILURE);
	}

	status = setresuid(1, 1, 1);
	if (status != 0) {
		perror("setresuid");
		exit(EXIT_FAILURE);
	}

	status = getresuid(&ruid, &euid, &suid);
	if (status != 0 || ruid != 1 || euid != 1 || suid != 1) {
		perror("getresuid");
		fprintf(stderr, "%ld %ld %ld\n", (unsigned long) ruid, (unsigned long) euid, (unsigned long) suid);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
