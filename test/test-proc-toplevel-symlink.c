/*
 * Regression test for readlink(2) of a top-level "/proc" entry, eg.
 * "/proc/self", "/proc/mounts", "/proc/thread-self", "/proc/net".
 *
 * detranslate_path() special-cases any readlink(2) referrer under
 * "/proc" by calling readlink_proc2(), which strips the last path
 * component off the referrer to get a "base" and then hardcoded
 * PATH1_IS_PREFIX as base's relationship to "/proc" when forwarding to
 * readlink_proc(). That's only true when the referrer has two or more
 * path segments under /proc (eg. "/proc/self/cwd" -> base
 * "/proc/self"). For a direct child of /proc, stripping the last
 * component leaves base as "/proc" itself -- PATHS_ARE_EQUAL, not
 * PATH1_IS_PREFIX -- which used to trip an assertion in
 * readlink_proc() and abort the tracer (or, in a non-assert build,
 * misparse a pid out of adjacent stack memory).
 *
 * The crash only manifests for a referrer whose target is itself an
 * absolute path (eg. "/proc/device-tree" -> "/sys/firmware/..." on
 * ARM systems with device-tree firmware data) -- detranslate_path()
 * has an earlier guard that skips any non-absolute-looking target
 * before ever reaching the /proc-specific code, and every top-level
 * /proc entry on a generic x86_64 machine (self, mounts, net,
 * thread-self) happens to have a relative target. There is no way to
 * construct a real, on-disk, absolute-target top-level /proc entry
 * from userspace, so this test cannot reproduce the crash itself; it
 * instead verifies the surrounding, always-reachable mechanism
 * (readlink of a top-level /proc entry, relative-target case) keeps
 * behaving correctly, as a regression guard for this code.
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int check_relative_target(const char *path, const char *expected)
{
    char target[PATH_MAX];
    ssize_t len;

    len = readlink(path, target, sizeof(target) - 1);
    if (len < 0) {
	if (errno == ENOENT) {
	    fprintf(stderr, "%s: does not exist here, skipping\n", path);
	    return 0;
	}
	fprintf(stderr, "readlink(%s): %s\n", path, strerror(errno));
	return -1;
    }
    target[len] = '\0';

    if (strcmp(target, expected) != 0) {
	fprintf(stderr, "readlink(%s): got '%s', expected '%s'\n",
		path, target, expected);
	return -1;
    }

    return 0;
}

int main(void)
{
    char self_target[PATH_MAX];
    char expected_pid[32];
    char expected_thread_self[64];
    ssize_t len;
    pid_t pid;

    pid = getpid();

    /* "/proc/self" -> "<pid>" (a bare number, no leading '/'). */
    len = readlink("/proc/self", self_target, sizeof(self_target) - 1);
    if (len < 0) {
	perror("readlink(/proc/self)");
	exit(EXIT_FAILURE);
    }
    self_target[len] = '\0';

    snprintf(expected_pid, sizeof(expected_pid), "%d", (int) pid);
    if (strcmp(self_target, expected_pid) != 0) {
	fprintf(stderr, "readlink(/proc/self): got '%s', expected '%s'\n",
		self_target, expected_pid);
	exit(EXIT_FAILURE);
    }

    /* "/proc/mounts" -> "self/mounts" (relative, untouched). */
    if (check_relative_target("/proc/mounts", "self/mounts") < 0)
	exit(EXIT_FAILURE);

    /* "/proc/net" -> "self/net" (relative, untouched). */
    if (check_relative_target("/proc/net", "self/net") < 0)
	exit(EXIT_FAILURE);

    /* "/proc/thread-self" -> "<pid>/task/<tid>"; single-threaded, so
     * tid == pid here. */
    snprintf(expected_thread_self, sizeof(expected_thread_self),
	     "%d/task/%d", (int) pid, (int) pid);
    if (check_relative_target("/proc/thread-self", expected_thread_self) <
	0)
	exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}
