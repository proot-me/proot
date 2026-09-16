/*
 * Regression test for readlink(2) of a top-level "/proc" entry, eg.
 * "/proc/self", "/proc/mounts", "/proc/thread-self", "/proc/net".
 *
 * readlink_proc2() used to mishandle these (see the fix in proc.c),
 * but only for entries with an absolute target (eg.
 * "/proc/device-tree" on ARM systems with device-tree firmware) --
 * none of which exist on generic x86_64 hardware. This test covers
 * the always-reachable relative-target case instead, as a general
 * regression guard.
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
