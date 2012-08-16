/*
 * Submitted-by: Thomas P. HIGDON <thomas.p.higdon@gmail.com>
 * Ref.: https://groups.google.com/d/msg/proot_me/4WbUndy-aXI/lmKiDfoIK_IJ
 */

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main()
{
	int status;
	struct timeval times[2] = {
		{.tv_sec = 52353, .tv_usec = 0},
		{ .tv_sec = 52353, .tv_usec = 0 } };
	char tmp[] = "proot-XXXXXX";

	mktemp(tmp);
	if (tmp[0] == '\0')
		exit(EXIT_FAILURE);

	(void) unlink(tmp);

	status = symlink("/etc/fstab", tmp);
	if (status < 0)
		exit(EXIT_FAILURE);

	status = lutimes(tmp, times);
	exit(status < 0 && errno != ENOSYS ? EXIT_FAILURE : EXIT_SUCCESS);
}

