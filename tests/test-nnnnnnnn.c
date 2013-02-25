#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
	const char *sockname = "/test-nnnnnnnn-socket";
	struct sockaddr_un sockaddr;
	socklen_t socklen;
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

	status = bind(fd, (const struct sockaddr *) &sockaddr, sizeof(sockaddr));
	if (status < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	bzero(&sockaddr, sizeof(sockaddr));

	socklen = sizeof(sockaddr);
	status = getsockname(fd, (struct sockaddr *) &sockaddr, &socklen);
	if (status < 0) {
		perror("getsockname");
		exit(EXIT_FAILURE);
	}

	if (sockaddr.sun_family != AF_UNIX) {
		fprintf(stderr, "! AF_UNIX\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(sockaddr.sun_path, sockname) != 0) {
		fprintf(stderr, "! %s\n", sockname);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
