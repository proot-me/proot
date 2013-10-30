#include <stdio.h>     /* rename(2), */
#include <stdlib.h>    /* atoi */
#include <unistd.h>    /* symlink(2), symlinkat(2), readlink(2), lstat(2), unlink(2), unlinkat(2)*/
#include <string.h>    /* str*, strrchr, strcat, strcpy, strncpy, strncmp */
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

#define PREFIX ".l2s."


/**
 * Move the path pointed to by @tracee's @sysarg to a new location,
 * symlink the original path to this new one, make @tracee's @sysarg
 * point to the new location.  This function returns -errno if an
 * error occured, otherwise 0.
 */
static int move_and_symlink_path(Tracee *tracee, Reg sysarg)
{
	char original[PATH_MAX];
	char intermediate[PATH_MAX];
	char final[PATH_MAX];
	char new_final[PATH_MAX];
	char * name;
	struct stat statl;
	ssize_t size;
	int status;
	int link_count;
	int first_link = 1;

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

	/* Check if it is a symbolic link.  */
	if (S_ISLNK(statl.st_mode)) {
		/* get name */
		size = readlink(original, intermediate, PATH_MAX);
		if (size < 0)
			return size;
		if (size >= PATH_MAX)
			return -ENAMETOOLONG;
		intermediate[size] = '\0';

		name = strrchr(intermediate, '/') + 1;

		if (strncmp(name, PREFIX, strlen(PREFIX)) == 0)
			first_link = 0;
	} else {
		/* compute new name */
		if (strlen(PREFIX) + strlen(original) + 5 >= PATH_MAX)
			return -ENAMETOOLONG;

		name = strrchr(original,'/') + 1;
		strncpy(intermediate, original, strlen(original) - strlen(name));
		intermediate[strlen(original) - strlen(name)] = '\0';
		strcat(intermediate, PREFIX);
		strcat(intermediate, name);
	}

	if (first_link) {
		/*Move the original content to the new path. */
		strcpy(final, intermediate);
		strcat(final, ".0002");
		status = rename(original, final);
		if (status < 0)
			return status;

		/* Symlink the intermediate to the final file.  */
		status = symlink(final, intermediate);
		if (status < 0)
			return status;	

		/* Symlink the original path to the intermediate one.  */
        	status = symlink(intermediate, original);
        	if (status < 0)
			return status;
	} else {
		/*Move the original content to new location, by incrementing count at end of path. */
		size = readlink(intermediate, final, PATH_MAX);
		if (size < 0)
			return size;
		if (size >= PATH_MAX)
			return -ENAMETOOLONG;
		final[size] = '\0';

		link_count = atoi(final + strlen(final) - 4);
		link_count++;

		strncpy(new_final, final, strlen(final) - 4);
		sprintf(new_final + strlen(final) - 4, "%04d", link_count);		
		
		status = rename(final, new_final);
		if (status < 0)
			return status;
		strcpy(final, new_final);
		/* Symlink the intermediate to the final file.  */
		status = unlink(intermediate);
		if (status < 0)
			return status;
		status = symlink(final, intermediate);
		if (status < 0)
			return status;
	}
	
	status = set_sysarg_path(tracee, intermediate, sysarg);
	if (status < 0)
		return status;

	return 0;
}


/* If path points a file that is a symlink to a file that begins
 *   with PREFIX, let the file be deleted, but also delete the 
 *   symlink that was created and decremnt the count that is tacked
 *   to end of original file.
 */
static int decrement_link_count(Tracee *tracee, Reg sysarg)
{
	char original[PATH_MAX];
	char intermediate[PATH_MAX];
	char final[PATH_MAX];
	char new_final[PATH_MAX];
	char * name;
	struct stat statl;
	ssize_t size;
	int status;
	int link_count;

	/* Note: this path was already canonicalized.  */
	size = read_string(tracee, original, peek_reg(tracee, CURRENT, sysarg), PATH_MAX);
	if (size < 0)
		return size;
	if (size >= PATH_MAX)
		return -ENAMETOOLONG;

	/* Check if it is a converted link already.  */
	status = lstat(original, &statl);
	if (S_ISLNK(statl.st_mode)) {
		size = readlink(original, intermediate, PATH_MAX);
		if (size < 0)
			return size;
		if (size >= PATH_MAX)
			return -ENAMETOOLONG;
		intermediate[size] = '\0';

		name = strrchr(intermediate, '/') + 1;

                /* Check if an l2s file is pointed to */
		if (strncmp(name, PREFIX, strlen(PREFIX)) == 0) {
			size = readlink(intermediate, final, PATH_MAX);
			if (size < 0)
				return size;
			if (size >= PATH_MAX)
				return -ENAMETOOLONG;
			final[size] = '\0';

			link_count = atoi(final + strlen(final) - 4);
			link_count--;
			
			/* Check if it is or is not the last link to delete */
			if (link_count > 0) {
				strncpy(new_final, final, strlen(final) - 4);
				sprintf(new_final + strlen(final) - 4, "%04d", link_count);		
		
				status = rename(final, new_final);
				if (status < 0)
					return status;				
				
				strcpy(final, new_final);

				/* Symlink the intermediate to the final file.  */
				status = unlink(intermediate);
				if (status < 0)
					return status;

				status = symlink(final, intermediate);
				if (status < 0)
					return status;				
			} else {
				/* If it is the last, delete the intermediate and final */
				status = unlink(intermediate);
				if (status < 0)
					return status;
				status = unlink(final);
				if (status < 0)
					return status;
			}
		}
	}

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
			{ PR_unlink, FILTER_SYSEXIT },
			{ PR_unlinkat, FILTER_SYSEXIT },
			FILTERED_SYSNUM_END,
		};
		extension->filtered_sysnums = filtered_sysnums;
		return 0;
	}

	case SYSCALL_ENTER_END: {
		Tracee *tracee = TRACEE(extension);

		switch (get_sysnum(tracee, ORIGINAL)) {
		case PR_unlink:
			/* If path points a file that is an symlink to a file that begins
			 *   with PREFIX, let the file be deleted, but also decrement the
			 *   hard link count, if it is greater than 1, otherwise delete
			 *   the original file and intermediate file too.
			 */

			status = decrement_link_count(tracee, SYSARG_1);
			if (status < 0)
				return status;

			break;

		case PR_unlinkat:
			/* If path points a file that is a symlink to a file that begins
			 *   with PREFIX, let the file be deleted, but also delete the 
			 *   symlink that was created and decremnt the count that is tacked
        		 *   to end of original file.
			 */

			status = decrement_link_count(tracee, SYSARG_2);
			if (status < 0)
				return status;

			break;

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
