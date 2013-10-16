#include <stdio.h>     /* rename(2), */
#include <unistd.h>    /* symlink(2), readlink(2), lstat(2), */
#include <string.h>    /* str*, */
#include <sys/types.h> /* lstat(2), */
#include <sys/stat.h>  /* lstat(2), */
#include <errno.h>     /* E*, */
#include <limits.h>    /* PATH_MAX, */

#include "extension/extension.h"
#include "tracee/tracee.h"
#include "tracee/mem.h"
#include "syscall/syscall.h"
#include "syscall/sysnum.h"
#include "arch.h"
#include "attribute.h"

#define SUFFIX ".l2s"

/**
 * Move the path pointed to by @tracee's @sysarg to a new location,
 * symlink the original path to this new one, make @tracee's @sysarg
 * point to the new location.  This function returns -errno if an
 * error occured, otherwise 0.
 */
static int move_and_symlink_path(Tracee *tracee, Reg sysarg)
{
	char original[PATH_MAX];
	char moved[PATH_MAX];
	struct stat statl;
	size_t length;
	ssize_t size;
	int status;

	/* Note: this path was already canonicalized.  */
	size = read_string(tracee, original, peek_reg(tracee, CURRENT, sysarg), PATH_MAX);
	if (size < 0)
		return size;
	if (size >= PATH_MAX)
		return -ENAMETOOLONG;

	/* Sanity check: directories can't be linked.  */
	status = lstat(original, &statl);
	if (status < 0)
		return status;
	if (S_ISDIR(statl.st_mode))
		return -EPERM;

	length = strlen(original);

	/* Check if it is a converted link already.  */
	if (strcmp(original + length - strlen(SUFFIX), SUFFIX) == 0) {
		size = readlink(original, moved, PATH_MAX);
		if (size < 0)
			return size;
		if (size >= PATH_MAX)
			return -ENAMETOOLONG;
	}
	else {
		/* Compute the new path.  */
		if (strlen(SUFFIX) + strlen(original) >= PATH_MAX)
			return -ENAMETOOLONG;
		strcpy(moved, original);
		strcat(moved, SUFFIX);

		/* Move the original content to the new path.  */
		status = rename(original, moved);
		if (status < 0)
			return status;

		/* Symlink the original path back to the new one.  */
		status = symlink(moved, original);
		if (status < 0)
			return status;
	}

	status = set_sysarg_path(tracee, moved, sysarg);
	if (status < 0)
		return status;

	return 0;
}

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int link2symlink_callback(Extension *extension, ExtensionEvent event,
				  intptr_t data1 UNUSED, intptr_t data2 UNUSED)
{
	int status;

	switch (event) {
	case INITIALIZATION: {
		/* List of syscalls handled by this extensions.  */
		static FilteredSysnum filtered_sysnums[] = {
			{ PR_link, FILTER_SYSEXIT },
			{ PR_linkat, FILTER_SYSEXIT },
			FILTERED_SYSNUM_END,
		};
		extension->filtered_sysnums = filtered_sysnums;
		return 0;
	}

	case SYSCALL_ENTER_END: {
		Tracee *tracee = TRACEE(extension);

		switch (get_sysnum(tracee, ORIGINAL)) {
		case PR_link:
			/* Convert:
			 *
			 *     int link(const char *oldpath, const char *newpath);
			 *
			 * into:
			 *
			 *     int symlink(const char *oldpath, const char *newpath);
			 */

			status = move_and_symlink_path(tracee, SYSARG_1);
			if (status < 0)
				return status;

			set_sysnum(tracee, PR_symlink);
			break;

		case PR_linkat:
			/* Convert:
			 *
			 *     int linkat(int olddirfd, const char *oldpath,
			 *                int newdirfd, const char *newpath, int flags);
			 *
			 * into:
			 *
			 *     int symlink(const char *oldpath, const char *newpath);
			 *
			 * Note: PRoot has already canonicalized
			 * linkat() paths this way:
			 *
			 *   olddirfd + oldpath -> oldpath
			 *   newdirfd + newpath -> newpath
			 */

			status = move_and_symlink_path(tracee, SYSARG_2);
			if (status < 0)
				return status;

			poke_reg(tracee, SYSARG_1, peek_reg(tracee, CURRENT, SYSARG_2));
			poke_reg(tracee, SYSARG_2, peek_reg(tracee, CURRENT, SYSARG_4));

			set_sysnum(tracee, PR_symlink);
			break;

		default:
			break;
		}
		return 0;
	}

	default:
		return 0;
	}
}
