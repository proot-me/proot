/*
 * Regression test for https://github.com/proot-me/proot/issues/182
 *
 * readlinkat(2) with an empty pathname (valid since Linux 2.6.39 when dirfd
 * was opened with O_PATH|O_NOFOLLOW) caused proot to call detranslate_path()
 * with an empty referrer string, which then passed length=0 to
 * compare_paths2, triggering assert(length2 > 0) and aborting the tracer.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    ssize_t len;
    int fd;
    const char *tmpdir = getenv("TMPDIR");

    if (tmpdir == NULL)
	tmpdir = "/tmp";

    snprintf(link_path, sizeof(link_path), "%s/test-bug-182-%d",
	     tmpdir, (int) getpid());

    /*
     * Symlink target is /tmp intentionally — readlinkat must return it
     * verbatim.
     */
    if (symlink("/tmp", link_path) < 0) {	/* NOSONAR: test creates
						 * deliberate symlink, not a
						 * temp file */
	perror("symlink");
	exit(EXIT_FAILURE);
    }

    /*
     * NOSONAR: TOCTOU is inherent to this test since
     * open(O_PATH|O_NOFOLLOW) is the syscall under test
     */
    fd = open(link_path, O_PATH | O_NOFOLLOW);
    if (fd < 0) {
	unlink(link_path);
	if (errno == EINVAL)
	    exit(125);		/* O_PATH unsupported, skip */
	perror("open");
	exit(EXIT_FAILURE);
    }

    /*
     * readlinkat with empty pathname operates on the symlink fd itself.
     * Before the fix this crashed proot via assert(length2 > 0) in
     * compare_paths2 because detranslate_path received an empty
     * referrer.
     */
    len = readlinkat(fd, "", target, sizeof(target) - 1);
    if (len < 0) {
	close(fd);
	unlink(link_path);
	if (errno == ENOENT || errno == EINVAL)
	    exit(125);		/* kernel doesn't support this
				 * feature, skip */
	perror("readlinkat");
	exit(EXIT_FAILURE);
    }
    target[len] = '\0';

    close(fd);
    unlink(link_path);

    if (strcmp(target, "/tmp") != 0) {	/* NOSONAR: /tmp is the
					 * expected symlink target,
					 * not a new temp file */
	fprintf(stderr, "readlinkat: unexpected target '%s'\n", target);
	exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
