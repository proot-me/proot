#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	int result;
	int status;
	char *path;
	int fd;

	path = strdup("/tmp/proot-test-iiiiiiii-XXXXXX");
	if (path == NULL) {
		result = 125;
		goto end;
	}

	mktemp(path);
	if (path[0] == '\0') {
		result = 125;
		goto end;
	}

	status = symlink("/this_shall_not_exist_outside_proot", path);
	if (status < 0) {
		result = 125;
		goto end;
	}

	/* For faccessat(2) and fchmodat(2) syscalls, the fourth
	 * parameter is *not* used by the kernel, only the libc uses
	 * it.  As a consequence, PRoot shall ignore this flag.
	 *
	 * To be sure this parameter is really ignored by PRoot, we
	 * set it to NOFOLLOW when performing a direct faccessat(2) to
	 * a symlink which is broken from the host point-of-view, but
	 * valid from a guest point-of-view.  When PRoot does not
	 * honor this flag, the faccessat(2) is performed against the
	 * referee anyway.
	 */
	status = syscall(SYS_faccessat, AT_FDCWD, path, X_OK, AT_SYMLINK_NOFOLLOW);
	result = (status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);

end:
	(void) unlink(path);
	exit(result);
}
