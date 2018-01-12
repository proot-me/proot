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

int create_rnd_socket(struct sockaddr_un *sockaddr)
{
	socklen_t socklen;
	char *sockname;
	mode_t mask;
	int status;
	int fd;
	sockname = strdup("proot-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
			"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxXXXXXX");
	if (sockname == NULL)
		return -1;

	(void) mktemp(sockname);
	if (sockname[0] == '\0')
		return -1;

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	bzero(sockaddr, sizeof(*sockaddr));
	sockaddr->sun_family = AF_UNIX;

	assert(strlen(sockname) == sizeof(sockaddr->sun_path));
	memcpy(sockaddr->sun_path, sockname, sizeof(sockaddr->sun_path));

	chdir("/tmp");

	(void) unlink(sockaddr->sun_path);
	status = bind(fd, (const struct sockaddr *) sockaddr, sizeof(*sockaddr));
	if (status < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	return fd;
}

int main()
{
	struct sockaddr_un sockaddr_src, sockaddr_dst, sockaddr_read;
	socklen_t socklen;
	char *sockname;
	mode_t mask;
	int status;
	int fd_src, fd_dst;
	char buffer[16];
	int len;
	char *str = "foobar";
	int str_len = strlen(str);

	fd_src = create_rnd_socket(&sockaddr_src);
	fd_dst = create_rnd_socket(&sockaddr_dst);
	if (fd_src == -1 || fd_src == -1)
		return 125;

	/* Test one roundtrip and make sure addr is translated correctly */
	status = sendto(fd_src, str, str_len, 0,
				(struct sockaddr *)&sockaddr_dst, sizeof(sockaddr_dst));
	if (status == -1 ) {
		perror("sendto");
		exit(EXIT_FAILURE);
	}

	socklen = sizeof(sockaddr_src);
	status = recvfrom(fd_dst, buffer, 16, 0,
				(struct sockaddr *)&sockaddr_read, &socklen);
	if (status == -1 ) {
		perror("recvfrom");
		exit(EXIT_FAILURE);
	}
	if(sockaddr_read.sun_family != sockaddr_src.sun_family ||
		!strncmp(sockaddr_read.sun_path, sockaddr_src.sun_path, sizeof(sockaddr_src.sun_path))) {
		fprintf(stderr, "recvfrom: received address differ from src\n");
		exit(EXIT_FAILURE);
	}
	if (status != str_len || strncmp(buffer, str, str_len)) {
		fprintf(stderr, "recvfrom: expected %s, received len=%d %s %d\n", str, status, buffer, strncmp(buffer, str, str_len));
		exit(EXIT_FAILURE);
	}

	/* Now the same, but using NULL parameter for recvfrom: */
	status = sendto(fd_src, str, str_len, 0,
				(struct sockaddr *)&sockaddr_dst, sizeof(sockaddr_dst));
	if (status == -1 ) {
		perror("sendto2");
		exit(EXIT_FAILURE);
	}
	status = recvfrom(fd_dst, buffer, 16, 0, NULL, NULL);
	if (status == -1 ) {
		perror("recvfrom NULL");
		exit(EXIT_FAILURE);
	}
	if (status != str_len || strncmp(buffer, str, str_len)) {
		fprintf(stderr, "recvfrom: expected %s, received len=%d %s %d\n", str, status, buffer, strncmp(buffer, str, str_len));
		exit(EXIT_FAILURE);
	}


	(void) unlink(sockaddr_src.sun_path);
	(void) unlink(sockaddr_dst.sun_path);
	exit(EXIT_SUCCESS);
}
