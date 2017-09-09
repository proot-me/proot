#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>

int main()
{
	struct sockaddr_un sockaddr;
	socklen_t socklen;
	char *sockname;
	mode_t mask;
	int status;
	int fd;

	sockname = strdup("proot-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
			"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxXXXXXX");
	if (sockname == NULL)
		return 125;

	(void) mktemp(sockname);
	if (sockname[0] == '\0')
		return 125;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	bzero(&sockaddr, sizeof(sockaddr));
	sockaddr.sun_family = AF_UNIX;

	assert(strlen(sockname) == sizeof(sockaddr.sun_path));
	memcpy(sockaddr.sun_path, sockname, sizeof(sockaddr.sun_path));

	chdir("/tmp");

	(void) unlink(sockaddr.sun_path);
	status = bind(fd, (const struct sockaddr *) &sockaddr, sizeof(sockaddr));
	if (status < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	(void) unlink(sockaddr.sun_path);
	exit(EXIT_SUCCESS);
}
