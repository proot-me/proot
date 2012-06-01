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
#include <stdio.h>     /* snprintf(3), */
#include <stdlib.h>    /* atoi(3), strtol(3), */

#include "path/canon.h"
#include "path/path.h"
#include "path/binding.h"

/* Action to do after a call to readlink_proc().  */
enum action {
	DEFAULT        = 0,    /* Nothing special to do, threat it as a regular link.  */
	CANONICALIZE   = 1,    /* The symlink was dereferenced, now canonicalize it.  */
	DONT_CANONICALIZE = 2, /* The symlink shouldn't be dereferenced nor canonicalized.  */
};

/**
 * This function emulates the @result of readlink("@path/@component")
 * with respect to @tracee, where @path belongs to "/proc" (according
 * to @comparison).  This function returns -errno on error, an enum
 * @action otherwise (c.f. above).
 *
 * Unlike readlink(), this function includes the nul terminating byte
 * to @result.
 */
static enum action readlink_proc(char result[PATH_MAX], const char path[PATH_MAX],
			const char component[NAME_MAX], struct tracee_info *tracee,
			enum path_comparison comparison)
{
	struct tracee_info *known_tracee;
	char proc_path[64]; /* 64 > sizeof("/proc//fd/") + 2 * sizeof(#ULONG_MAX) */
	int status;
	pid_t pid;

	/* Remember: comparison = compare_paths("/proc", path)  */
	switch (comparison) {
	case PATHS_ARE_EQUAL:
		/* Substitute "/proc/self" with "/proc/<PID>".  */
		if (strcmp(component, "self") != 0)
			return DEFAULT;

		status = snprintf(result, PATH_MAX, "/proc/%d", tracee->pid);
		if (status < 0 || status >= PATH_MAX)
			return -EPERM;

		return CANONICALIZE;

	case PATH1_IS_PREFIX:
		/* Handle "/proc/<PID>" below, where <PID> is process
		 * monitored by PRoot.  */
		break;

	default:
		return DEFAULT;
	}

	pid = atoi(path + strlen("/proc/"));
	if (pid == 0)
		return DEFAULT;

	known_tracee = get_tracee_info(pid, false);
	if (!known_tracee)
		return DEFAULT;

	/* Handle links in "/proc/<PID>/".  */
	status = snprintf(proc_path, sizeof(proc_path), "/proc/%d", pid);
	if (status < 0 || status >= sizeof(proc_path))
		return -EPERM;

	comparison = compare_paths(proc_path, path);
	switch (comparison) {
	case PATHS_ARE_EQUAL:
#if 0
#define SUBSTITUTE(name)					\
		do {						\
			if (strcmp(component, #name) == 0	\
				&& tracee->name == NULL) {	\
				status = strlen(tracee->name);	\
				if (status >= PATH_MAX)		\
					return -EPERM;		\
				strcpy(result, tracee->name);	\
				return CANONICALIZE;		\
			}					\
		} while (0)

		/* Substitute link "/proc/<PID>/???" with the content
		 * of tracee->???.  */
		SUBSTITUTE(exe);
		SUBSTITUTE(root);
		SUBSTITUTE(cwd);

		return DEFAULT;
#undef SUBSTITUTE
#endif
	case PATH1_IS_PREFIX:
		/* Handle "/proc/<PID>/???" below.  */
		break;

	default:
		return DEFAULT;
	}

	/* Handle links in "/proc/<PID>/fd/".  */
	status = snprintf(proc_path, sizeof(proc_path), "/proc/%d/fd", pid);
	if (status < 0 || status >= sizeof(proc_path))
		return -EPERM;

	comparison = compare_paths(proc_path, path);
	switch (comparison) {
	case PATHS_ARE_EQUAL:
		/* Sanity check: a number is expected.  */
		errno = 0;
		(void) strtol(component, (char **) NULL, 10);
		if (errno != 0)
			return -EPERM;

		/* Don't dereference "/proc/<PID>/fd/???" now: they
		 * can point to anonymous pipe, socket, ...  otherwise
		 * they point to a path already canonicalized by the
		 * kernel.
		 *
		 * Note they are still correctly detranslated in
		 * syscall/exit.c if a monitored process uses
		 * readlink() against any of them.  */
		status = snprintf(result, PATH_MAX, "%s/%s", path, component);
		if (status < 0 || status >= PATH_MAX)
			return -EPERM;

		return DONT_CANONICALIZE;

	default:
		return DEFAULT;
	}

	return DEFAULT;
}

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
int canonicalize(struct tracee_info *tracee, const char *fake_path, bool deref_final,
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
		enum path_comparison comparison;
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

		/* Check which kind of directory we have to
		   canonicalize; either a binding or a
		   translatable one. */
		status = join_paths(2, tmp, result, component);
		if (status < 0)
			return status;

		if (substitute_binding(GUEST_SIDE, tmp) >= 0) {
			strcpy(real_entry, tmp);
		}
		else {
			status = join_paths(2, real_entry, root, tmp);
			if (status < 0)
				return status;
		}

		status = lstat(real_entry, &statl);

		/* Return an error if a non-final component isn't a
		 * directory nor a symlink.  The error is "No such
		 * file or directory" if this component doesn't exist,
		 * otherwise the error is "Not a directory".  This
		 * sanity check is bypassed if the canonicalization is
		 * made in the name of PRoot itself (!tracee) since
		 * this would return a spurious error during the
		 * creation of "deep" guest binding paths.  */
		if (!is_final && tracee != NULL &&
		    !S_ISDIR(statl.st_mode) && !S_ISLNK(statl.st_mode))
			return (status < 0 ? -ENOENT : -ENOTDIR);

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
		comparison = compare_paths("/proc", result);
		switch (comparison) {
		case PATHS_ARE_EQUAL:
		case PATH1_IS_PREFIX:
			/* Some links in "/proc" are generated
			 * dynamically by the kernel.  PRoot has to
			 * emulate some of them.  */
			status = readlink_proc(tmp, result, component, tracee, comparison);
			if (status < 0)
				return status;
			else if (status == DONT_CANONICALIZE) {
				strcpy(result, tmp);
				return 0;
			}
			else if (status == CANONICALIZE)
				break;
			else
				; /* DEFAULT: Fall through.  */
		default:
			status = readlink(real_entry, tmp, sizeof(tmp));
			if (status < 0)
				return status;
			else if (status == sizeof(tmp))
				return -ENAMETOOLONG;
			tmp[status] = '\0';

			/* Remove the leading "root" part if needed, it's
			 * useful for "/proc/self/cwd/" for instance. */
			status = detranslate_path(tmp, real_entry);
			if (status < 0)
				return status;
		}

		/* Canonicalize recursively the referee in case it
		   is/contains a link, moreover if it is not an
		   absolute link then it is relative to 'result'. */
		status = canonicalize(tracee, tmp, true, result, ++nb_readlink);
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
