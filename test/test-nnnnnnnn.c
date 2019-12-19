#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main()
{
	const char *sockname = "/test-nnnnnnnn-socket";
	struct sockaddr_un sockaddr;
	socklen_t socklen;
	mode_t mask;
	int status;
	int fd;

	/* root can create $hostfs/test-nnnnnnnn-socket.  */
	if (getuid() == 0)
		return 125;

	/* clean-up previous socket.  */
	(void) unlink(sockname);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	bzero(&sockaddr, sizeof(sockaddr));
	sockaddr.sun_family = AF_UNIX;
	strcpy(sockaddr.sun_path, sockname);

	mask = umask(S_IXUSR|S_IXGRP|S_IRWXO);
	status = bind(fd, (const struct sockaddr *) &sockaddr, SUN_LEN(&sockaddr));
	if (status < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	umask(mask);

	status = listen(fd, 50);
	if (status < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	bzero(&sockaddr, sizeof(sockaddr));

	socklen = sizeof(sockaddr);
	status = getsockname(fd, (struct sockaddr *) &sockaddr, &socklen);
	if (status < 0) {
		perror("getsockname");
		exit(EXIT_FAILURE);
	}

	if (socklen != SUN_LEN(&sockaddr) + 1) {
		fprintf(stderr, "socklen: %d != %d + 1\n", socklen, SUN_LEN(&sockaddr));
		exit(EXIT_FAILURE);
	}

	if (sockaddr.sun_family != AF_UNIX) {
		fprintf(stderr, "! AF_UNIX\n");
		exit(EXIT_FAILURE);
	}

	if (socklen == sizeof(sockaddr) + 1)
		status = strncmp(sockaddr.sun_path, sockname, sizeof(sockaddr.sun_path));
	else
		status = strcmp(sockaddr.sun_path, sockname);

	if (status != 0) {
		fprintf(stderr, "! %s\n", sockname);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
