/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011, 2012 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <sys/types.h> /* pid_t */
#include <limits.h>    /* PATH_MAX, */
#include <sys/param.h> /* MAXSYMLINKS, */
#include <errno.h>     /* E*, */
#include <sys/stat.h>  /* lstat(2), S_ISREG(), */
#include <unistd.h>    /* access(2), lstat(2), */
#include <string.h>    /* string(3), */
#include <assert.h>    /* assert(3), */
#include <stdio.h>     /* sprintf(3), */

#include "path/canon.h"
#include "path/path.h"
#include "path/binding.h"

/**
 * Copy in @result the canonicalization (see `man 3 realpath`) of
 * @fake_path regarding to @root. The path to canonicalize could be
 * either absolute or relative to @result. When the last component of
 * @fake_path is a link, it is dereferenced only if @deref_final is
 * true -- it is useful for syscalls like lstat(2). The parameter
 * @nb_readlink should be set to 0 unless you know what you are
 * doing. This function returns -errno if an error occured, otherwise
 * it returns 0.
 */
int canonicalize(pid_t pid, const char *fake_path, int deref_final,
		char result[PATH_MAX], unsigned int nb_readlink)
{
	const char *cursor;
	char tmp[PATH_MAX];
	int is_final;
	int status;

	/* Avoid infinite loop on circular links. */
	if (nb_readlink > MAXSYMLINKS)
		return -ELOOP;

#ifdef BENCHMARK_TRACEE_HANDLING
	size_t result_length = strlen(result);
	size_t fake_length = strlen(fake_path);
	if (result_length + fake_length + 1 >= PATH_MAX)
		return -ENAMETOOLONG;
	else {
		result[result_length] = '/';
		strcpy(result + result_length + 1, fake_path);
		return 0;
	}
#endif

	/* Sanity checks. */
	assert(fake_path != NULL);
	assert(result != NULL);

	if (fake_path[0] != '/') {
		/* Ensure 'result' contains an absolute base of the relative fake path. */
		if (result[0] != '/')
			return -EINVAL;
	}
	else
		strcpy(result, "/");

	/* Canonicalize recursely 'fake_path' into 'result'. */
	cursor = fake_path;
	is_final = 0;
	while (!is_final) {
		char component[NAME_MAX];
		char real_entry[PATH_MAX];
		struct stat statl;

		status = next_component(component, &cursor);
		if (status < 0)
			return status;
		is_final = status;

		if (strcmp(component, ".") == 0) {
			if (is_final)
				is_final = FINAL_FORCE_DIR;
			continue;
		}

		if (strcmp(component, "..") == 0) {
			pop_component(result);
			if (is_final)
				is_final = FINAL_FORCE_DIR;
			continue;
		}

		/* Very special case: substitute "/proc/self" with "/proc/$pid".
		   The following check covers only 99.999% of the cases. */
		if (   strcmp(component, "self") == 0
		    && strcmp(result, "/proc")   == 0
		    && (!is_final || deref_final)) {
			status = sprintf(component, "%d", pid);
			if (status < 0)
				return -EPERM;
			if (status >= sizeof(component))
				return -EPERM;
		}

		/* Check which kind of directory we have to
		   canonicalize; either a binding or a
		   translatable one. */
		status = join_paths(2, tmp, result, component);
		if (status < 0)
			return status;

		if (substitute_binding(BINDING_LOCATION, tmp) >= 0) {
			strcpy(real_entry, tmp);
		}
		else {
			status = join_paths(2, real_entry, root, tmp);
			if (status < 0)
				return status;
		}

		status = lstat(real_entry, &statl);

		/* Nothing special to do if it's not a link or if we
		   explicitly ask to not dereference 'fake_path', as
		   required by syscalls like lstat(2). Obviously, this
		   later condition does not apply to intermediate path
		   components. Errors are explicitly ignored since
		   they should be handled by the caller. */
		if (status < 0
		    || !S_ISLNK(statl.st_mode)
		    || (is_final == FINAL_NORMAL && !deref_final)) {
			strcpy(tmp, result);
			status = join_paths(2, result, tmp, component);
			if (status < 0)
				return status;
			continue;
		}

		/* It's a link, so we have to dereference *and*
		   canonicalize to ensure we are not going outside the
		   new root. */
		status = readlink(real_entry, tmp, sizeof(tmp));
		if (status < 0)
			return status;
		else if (status == sizeof(tmp))
			return -ENAMETOOLONG;
		tmp[status] = '\0';

		/* Remove the leading "root" part if needed, it's
		 * useful for "/proc/self/cwd/" for instance. */
		status = detranslate_path(tmp, false, !belongs_to_guestfs(real_entry));
		if (status < 0)
			return status;

		/* Canonicalize recursively the referee in case it
		   is/contains a link, moreover if it is not an
		   absolute link so it is relative to 'result'. */
		status = canonicalize(pid, tmp, 1, result, ++nb_readlink);
		if (status < 0)
			return status;
	}

	/* Ensure we are accessing a directory. */
	if (is_final == FINAL_FORCE_DIR) {
		strcpy(tmp, result);
		status = join_paths(2, result, tmp, "");
		if (status < 0)
			return status;
	}

	return 0;
}
