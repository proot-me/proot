/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2013 STMicroelectronics
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

#include "path/canon.h"
#include "path/path.h"
#include "path/binding.h"
#include "path/glue.h"
#include "path/proc.h"
#include "extension/extension.h"

/**
 * Resolve bindings (if any) in @guest_path and copy the translated
 * path into @host_path.  Also, this function checks that a non-final
 * component is either a directory (returned value is 0) or a symlink
 * (returned value is 1), otherwise it returns -errno (-ENOENT or
 * -ENOTDIR).
 */
static inline int substitute_binding_stat(Tracee *tracee, Finality is_final,
					const char guest_path[PATH_MAX], char host_path[PATH_MAX])
{
	struct stat statl;
	int status;

	strcpy(host_path, guest_path);
	status = substitute_binding(tracee, GUEST, host_path);
	if (status < 0)
		return status;

	/* Don't notify extensions during the initialization of a binding.  */
	if (tracee->glue_type == 0) {
		status = notify_extensions(tracee, HOST_PATH, (intptr_t)host_path, is_final);
		if (status < 0)
			return status;
	}

	statl.st_mode = 0;
	status = lstat(host_path, &statl);

	/* Build the glue between the hostfs and the guestfs during
	 * the initialization of a binding.  */
	if (status < 0 && tracee->glue_type != 0) {
		statl.st_mode = build_glue(tracee, guest_path, host_path, is_final);
		if (statl.st_mode == 0)
			status = -1;
	}

	/* Return an error if a non-final component isn't a
	 * directory nor a symlink.  The error is "No such
	 * file or directory" if this component doesn't exist,
	 * otherwise the error is "Not a directory".  */
	if (!is_final && !S_ISDIR(statl.st_mode) && !S_ISLNK(statl.st_mode))
		return (status < 0 ? -ENOENT : -ENOTDIR);

	return (S_ISLNK(statl.st_mode) ? 1 : 0);
}

/**
 * Copy in @guest_path the canonicalization (see `man 3 realpath`) of
 * @user_path regarding to @tracee->root.  The path to canonicalize
 * could be either absolute or relative to @guest_path. When the last
 * component of @user_path is a link, it is dereferenced only if
 * @deref_final is true -- it is useful for syscalls like lstat(2).
 * The parameter @recursion_level should be set to 0 unless you know
 * what you are doing. This function returns -errno if an error
 * occured, otherwise it returns 0.
 */
int canonicalize(Tracee *tracee, const char *user_path, bool deref_final,
		 char guest_path[PATH_MAX], unsigned int recursion_level)
{
	char scratch_path[PATH_MAX];
	bool pending_dot = false;
	Finality is_final;
	const char *cursor;
	int status;

	/* Avoid infinite loop on circular links.  */
	if (recursion_level > MAXSYMLINKS)
		return -ELOOP;

	/* Sanity checks.  */
	assert(user_path != NULL);
	assert(guest_path != NULL);
	assert(user_path != guest_path);

	if (user_path[0] != '/') {
		/* Ensure 'guest_path' contains an absolute base of
		 * the relative `user_path`.  */
		if (guest_path[0] != '/')
			return -EINVAL;
	}
	else
		strcpy(guest_path, "/");

	/* Canonicalize recursely 'user_path' into 'guest_path'.  */
	cursor = user_path;
	is_final = NOT_FINAL;
	while (!is_final) {
		Comparison comparison;
		char component[NAME_MAX];
		char host_path[PATH_MAX];

		status = next_component(component, &cursor);
		if (status < 0)
			return status;
		if (status == FINAL_SLASH && pending_dot)
			is_final = FINAL_DOT;
		else
			is_final = status;

		if (strcmp(component, ".") == 0) {
			if (is_final)
				is_final = FINAL_DOT;
			pending_dot = true;
			continue;
		}
		pending_dot = false;

		if (strcmp(component, "..") == 0) {
			pop_component(guest_path);
			if (is_final)
				is_final = FINAL_SLASH;
			continue;
		}

		status = join_paths(2, scratch_path, guest_path, component);
		if (status < 0)
			return status;

		/* Resolve bindings and check that a non-final
		 * component exists and either is a directory or is a
		 * symlink.  For this latter case, we check that the
		 * symlink points to a directory once it is
		 * canonicalized, at the end of this loop.  */
		status = substitute_binding_stat(tracee, is_final, scratch_path, host_path);
		if (status < 0)
			return status;

		/* Nothing special to do if it's not a link or if we
		 * explicitly ask to not dereference 'user_path', as
		 * required by syscalls like lstat(2). Obviously, this
		 * later condition does not apply to intermediate path
		 * components.  Errors are explicitly ignored since
		 * they should be handled by the caller. */
		if (status <= 0 || (is_final == FINAL_NORMAL && !deref_final)) {
			strcpy(scratch_path, guest_path);
			status = join_paths(2, guest_path, scratch_path, component);
			if (status < 0)
				return status;
			continue;
		}

		/* It's a link, so we have to dereference *and*
		 * canonicalize to ensure we are not going outside the
		 * new root.  */
		comparison = compare_paths("/proc", guest_path);
		switch (comparison) {
		case PATHS_ARE_EQUAL:
		case PATH1_IS_PREFIX:
			/* Some links in "/proc" are generated
			 * dynamically by the kernel.  PRoot has to
			 * emulate some of them.  */
			status = readlink_proc(tracee, scratch_path,
					       guest_path, component, comparison);
			switch (status) {
			case CANONICALIZE:
				/* The symlink is already dereferenced,
				 * now canonicalize it.  */
				goto canon;

			case DONT_CANONICALIZE:
				/* If and only very final, this symlink
				 * shouldn't be dereferenced nor canonicalized.  */
				if (is_final == FINAL_NORMAL && recursion_level == 0) {
					strcpy(guest_path, scratch_path);
					return 0;
				}
				break;

			default:
				if (status < 0)
					return status;
			}

		default:
			break;
		}

		status = readlink(host_path, scratch_path, sizeof(scratch_path));
		if (status < 0)
			return status;
		else if (status == sizeof(scratch_path))
			return -ENAMETOOLONG;
		scratch_path[status] = '\0';

		/* Remove the leading "root" part if needed, it's
		 * useful for "/proc/self/cwd/" for instance.  */
		status = detranslate_path(tracee, scratch_path, host_path);
		if (status < 0)
			return status;

	canon:
		/* Canonicalize recursively the referee in case it
		 * is/contains a link, moreover if it is not an
		 * absolute link then it is relative to
		 * 'guest_path'. */
		status = canonicalize(tracee, scratch_path, true, guest_path, recursion_level + 1);
		if (status < 0)
			return status;

		/* Check that a non-final canonicalized/dereferenced
		 * symlink exists and is a directory.  */
		status = substitute_binding_stat(tracee, is_final, guest_path, host_path);
		if (status < 0)
			return status;

		/* Here, 'guest_path' shouldn't be a symlink anymore.  */
		assert(status != 1);
	}

	/* At the exit stage of the first level of recursion,
	 * `guest_path` is fully canonicalized but a terminating '/'
	 * or a terminating '.' may be required to keep the initial
	 * semantic of `user_path`.  */
	if (recursion_level == 0) {
		switch (is_final) {
		case FINAL_NORMAL:
			break;

		case FINAL_SLASH:
			strcpy(scratch_path, guest_path);
			status = join_paths(2, guest_path, scratch_path, "");
			if (status < 0)
				return status;
			break;

		case FINAL_DOT:
			strcpy(scratch_path, guest_path);
			status = join_paths(2, guest_path, scratch_path, ".");
			if (status < 0)
				return status;
			break;

		default:
			assert(0);
		}
	}

	return 0;
}
