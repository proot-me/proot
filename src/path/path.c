/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 * Inspired by: realpath() from the GNU C Library.
 */

#define _ATFILE_SOURCE
#include <unistd.h>   /* readlink(2), lstat(2), readlinkat(2) */
#include <assert.h>   /* assert(3), */
#include <string.h>   /* strcmp(3), strcpy(3), strncpy(3), memmove(3), */
#include <limits.h>   /* PATH_MAX, */
#include <sys/stat.h> /* lstat(2), S_ISLNK(), */
#include <stdlib.h>   /* realpath(3), exit(3), EXIT_*, */
#include <errno.h>    /* errno(3), */
#include <stdarg.h>   /* va_*(3), */
#include <sys/param.h> /* MAXSYMLINKS, */
#include <stddef.h>   /* ptrdiff_t, */
#include <stdio.h>    /* sprintf(3), */
#include <sys/types.h> /* pid_t, */
#include <dirent.h>   /* readdir(3), */

#include "path/path.h"
#include "notice.h"
#include "syscall/syscall.h"
#include "config.h"

#include "compat.h"

static int initialized = 0;
static char root[PATH_MAX];
static size_t root_length;
static int use_runner = 0;

struct binding_path {
	char path[PATH_MAX];
	size_t length;
};

struct binding {
	struct binding_path real;
	struct binding_path location;

	int sanitized;
	int need_substitution;

	struct binding *next;
};

static struct binding *bindings = NULL;

/**
 * Save @path in the list of paths that are bound for the
 * translation mechanism.
 */
void bind_path(const char *path, const char *location)
{
	struct binding *binding;
	const char *tmp;

	binding = calloc(1, sizeof(struct binding));
	if (binding == NULL) {
		notice(WARNING, SYSTEM, "calloc()");
		return;
	}

	if (realpath(path, binding->real.path) == NULL) {
		notice(WARNING, SYSTEM, "realpath(\"%s\")", path);
		goto error;
	}

	binding->real.length = strlen(binding->real.path);

	/* Special case when the real rootfs is bound. */
	if (binding->real.length == 1)
		binding->real.length = 0;

	tmp = location ? location : path;
	if (strlen(tmp) >= PATH_MAX) {
		notice(ERROR, INTERNAL, "binding location \"%s\" is too long", tmp);
		goto error;
	}

	strcpy(binding->location.path, tmp);

	/* The sanitization of binding->location is delayed until
	 * init_module_path(). */
	binding->location.length = 0;
	binding->sanitized = 0;

	binding->next = bindings;
	bindings = binding;

	return;

error:
	free(binding);
	binding = NULL;
	return;
}

#define BINDING_LOCATION 1
#define BINDING_REAL     2

/**
 * Substitute the binding location (if any) with the real path in
 * @path.  This function returns:
 *
 *     * -1 if it is not a binding location
 *
 *     * 0 if it is a binding location but no substitution is needed
 *       ("symetric" binding)
 *
 *     * 1 if it is a binding location and a substitution was performed
 *       ("asymmetric" binding)
 */
static int substitute_binding(int which, char path[PATH_MAX])
{
	struct binding *binding;
	size_t path_length = strlen(path);

	for (binding = bindings; binding != NULL; binding = binding->next) {
		struct binding_path *ref;
		struct binding_path *anti_ref;

		if (!binding->sanitized)
			continue;

		switch (which) {
		case BINDING_LOCATION:
			ref      = &binding->location;
			anti_ref = &binding->real;
			break;

		case BINDING_REAL:
			ref      = &binding->real;
			anti_ref = &binding->location;
			break;

		default:
			notice(WARNING, INTERNAL, "unexpected value for binding reference");
			return -1;
		}

		/* Skip comparisons that obviously fail. */
		if (ref->length > path_length)
			continue;

		if (   path[ref->length] != '/'
		    && path[ref->length] != '\0')
			continue;

		if (which == BINDING_REAL) {
			/* Don't substitute systematically the prefix of the
			 * rootfs when used as an asymmetric binding, as with:
			 *
			 *     proot -m /usr:/location /usr/local/slackware
			 */
			if (strncmp(path, root, root_length) == 0)
				continue;

			/* Avoid an extra trailing '/' when in the
			 * asymmetric binding of the real rootfs. */
			if (ref->length == 0 &&	path_length == 1)
				path_length = 0;
		}

		/* Using strncmp(3) to compare paths works fine here
		 * because both pathes were sanitized, i.e. there is
		 * no redundant ".", ".." or "/". */
		if (strncmp(ref->path, path, ref->length) != 0)
			continue;

		/* Is it a "symetric" binding?  */
		if (binding->need_substitution == 0)
			return 0;

		/* Ensure the substitution will not create a buffer
		 * overflow. */
		if (path_length - ref->length + anti_ref->length >= PATH_MAX)  {
			notice(WARNING, INTERNAL, "Can't handle binding %s: pathname too long");
			return -1;
		}

		/* Replace the binding location with the real path. */
		memmove(path + anti_ref->length, path + ref->length, path_length - ref->length);
		memcpy(path, anti_ref->path, anti_ref->length);
		path[path_length - ref->length + anti_ref->length] = '\0';

		/* Special case when the real rootfs is bound at
		 * the location pointed to by path[]. */
		if (path[0] == '\0') {
			path[0] = '/';
			path[1] = '\0';
		}

		return 1;
	}

	return -1;
}

#define FINAL_NORMAL    1
#define FINAL_FORCE_DIR 2

/**
 * Copy in @component the first path component pointed to by @cursor,
 * this later is updated to point to the next component for a further
 * call. This function returns:
 *
 *     - -errno if an error occured.
 *
 *     - FINAL_FORCE_DIR if it the last component of the path but we
 *       really expect a directory.
 *
 *     - FINAL_NORMAL if it the last component of the path.
 *
 *     - 0 otherwise.
 */
static inline int next_component(char component[NAME_MAX], const char **cursor)
{
	const char *start;
	ptrdiff_t length;
	int want_dir;

	/* Sanity checks. */
	assert(component != NULL);
	assert(cursor    != NULL);

	/* Skip leading path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	/* Find the next component. */
	start = *cursor;
	while (**cursor != '\0' && **cursor != '/')
		(*cursor)++;
	length = *cursor - start;

	if (length >= NAME_MAX)
		return -ENAMETOOLONG;

	/* Extract the component. */
	strncpy(component, start, length);
	component[length] = '\0';

	/* Check if a [link to a] directory is expected. */
	want_dir = (**cursor == '/');

	/* Skip trailing path separators. */
	while (**cursor != '\0' && **cursor == '/')
		(*cursor)++;

	if (**cursor == '\0')
		return (want_dir
			? FINAL_FORCE_DIR
			: FINAL_NORMAL);

	return 0;
}

/**
 * Put an end-of-string ('\0') right before the last component of @path.
 */
static inline void pop_component(char *path)
{
	int offset;

	/* Sanity checks. */
	assert(path != NULL);

	offset = strlen(path) - 1;
	assert(offset >= 0);

	/* Don't pop over "/", it doesn't mean anything. */
	if (offset == 0) {
		assert(path[0] == '/' && path[1] == '\0');
		return;
	}

	/* Skip trailing path separators. */
	while (offset > 1 && path[offset] == '/')
		offset--;

	/* Search for the previous path separator. */
	while (offset > 1 && path[offset] != '/')
		offset--;

	/* Cut the end of the string before the last component. */
	path[offset] = '\0';
	assert(path[0] == '/');
}

/**
 * Copy in @result the concatenation of several paths (@number_paths)
 * and adds a path separator ('/') in between when needed. This
 * function returns -errno if an error occured, otherwise it returns 0.
 */
static inline int join_paths(int number_paths, char result[PATH_MAX], ...)
{
	va_list paths;
	size_t length;
	int i;

	result[0] = '\0';

	/* Parse the list of variadic arguments. */
	va_start(paths, result);
	length = 0;
	for (i = 0; i < number_paths; i++) {
		const char *path;
		int need_separator;
		size_t old_length;

		path = va_arg(paths, const char *);
		if (path == NULL)
			continue;

		/* Check if a path separator is needed. */
		if (length > 0 && result[length - 1] != '/' && path[0] != '/')
			need_separator = 1;
		else
			need_separator = 0;

		old_length = length;
		length += strlen(path) + need_separator;
		if (length >= PATH_MAX) {
			va_end(paths);
			return -ENAMETOOLONG;
		}

		if (need_separator != 0) {
			strcat(result + old_length, "/");
			old_length++;
		}
		strcat(result + old_length, path);
	}
	va_end(paths);

	return 0;
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
static int canonicalize(pid_t pid,
			const char *fake_path,
			int deref_final,
			char result[PATH_MAX],
			unsigned int nb_readlink)
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

		if (strcmp(component, ".") == 0)
			continue;

		if (strcmp(component, "..") == 0) {
			pop_component(result);
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
		status = detranslate_path(tmp, 0);
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

/**
 * Create a "dummy" path up to the canonicalized binding location
 * @c_path, it cheats programs that walk up to it.
 */
static void create_dummy(char c_path[PATH_MAX])
{
	char t_current_path[PATH_MAX];
	char t_path[PATH_MAX];
	struct stat statl;
	int status;
	int is_final;
	const char *cursor;

	status = join_paths(2, t_path, root, c_path);
	if (status < 0)
		goto error;

	status = lstat(t_path, &statl);
	if (status == 0)
		return;

	if (errno != ENOENT)
		goto error;

	/* Skip the "root" part since we know it exists. */
	strcpy(t_current_path, root);
	cursor = t_path + root_length;

	is_final = 0;
	while (!is_final) {
		char component[NAME_MAX];
		char tmp[PATH_MAX];

		status = next_component(component, &cursor);
		if (status < 0)
			goto error;
		is_final = status;

		strcpy(tmp, t_current_path);
		status = join_paths(2, t_current_path, tmp, component);
		if (status < 0)
			goto error;

		/* Note that even the final component is a directory,
		 * actually its type doesn't matter since only the
		 * entry in the parent directory is important to cheat
		 * "walkers". */
		status = mkdir(t_current_path, 0777);
		if (status < 0 && errno != EEXIST)
			goto error;
	}

	notice(INFO, USER, "create the binding location \"%s\"", c_path);
	return;

error:
	notice(WARNING, USER, "can't create the binding location \"%s\": "
	       "expect some troubles with programs that walk up to it", c_path);
}

/**
 * Initialize internal data of the path translator.
 */
void init_module_path(const char *new_root, int opt_runner)
{
	struct binding *binding;

	if (realpath(new_root, root) == NULL)
		notice(ERROR, SYSTEM, "realpath(\"%s\")", new_root);

	if (strcmp(root, "/") == 0)
		root_length = 0;
	else
		root_length = strlen(root);
	use_runner = opt_runner;
	initialized = 1;

	/* Now the module is initialized so we can call
	 * canonicalize() safely. */
	for (binding = bindings; binding != NULL; binding = binding->next) {
		char tmp[PATH_MAX];
		int status;

		assert(!binding->sanitized);

		strcpy(tmp, binding->location.path);
		binding->location.path[0] = '\0';

		/* Sanitize the location of the binding within the
		   alternate rootfs since it is assumed by
		   substitute_binding().  Note the real path is already
		   sanitized in bind_path().  */
		status = canonicalize(0, tmp, 1, binding->location.path, 0);
		if (status < 0) {
			notice(WARNING, INTERNAL, "sanitizing the binding location \"%s\": %s",
			       tmp, strerror(-status));
			goto error;
		}

		if (strcmp(binding->location.path, "/") == 0) {
			notice(WARNING, USER, "can't create a binding in \"/\"");
			goto error;
		}

		binding->need_substitution = strcmp(binding->real.path, binding->location.path);
		binding->location.length = strlen(binding->location.path);

		/* Remove the trailing slash as expected by substitute_binding(). */
		if (binding->location.path[binding->location.length - 1] == '/') {
			binding->location.path[binding->location.length - 1] = '\0';
			binding->location.length--;
		}

		create_dummy(binding->location.path);

		binding->sanitized = 1;

		VERBOSE(1, "binding \"%s\" in \"%s\"", binding->real.path, binding->location.path);
		continue;

	error:
		/* XXX TODO: remove this element from the list instead. */
		binding->sanitized = 0;
	}
}

/**
 * Check if the translation should be delayed.
 *
 * It is useful when using a runner that needs shared libraries or
 * reads some configuration files, for instance.
 */
static int is_delayed(struct tracee_info *tracee, const char *path)
{
	if (tracee->trigger == NULL)
		return 0;

	if (strcmp(tracee->trigger, path) != 0)
		return 1;

	free(tracee->trigger);
	tracee->trigger = NULL;

	return 0;
}

/**
 * Copy in @result the equivalent of @root + canonicalize(@dir_fd +
 * @fake_path).  If @fake_path is not absolute then it is relative to
 * the directory referred by the descriptor @dir_fd (AT_FDCWD is for
 * the current working directory).  See the documentation of
 * canonicalize() for the meaning of @deref_final.
 */
int translate_path(struct tracee_info *tracee, char result[PATH_MAX], int dir_fd, const char *fake_path, int deref_final)
{
	char link[32]; /* 32 > sizeof("/proc//cwd") + sizeof(#ULONG_MAX) */
	char tmp[PATH_MAX];
	int status;
	pid_t pid;

	assert(initialized != 0);

	pid = (tracee != NULL ? tracee->pid : getpid());

	/* Use "/" as the base if it is an absolute [fake] path. */
	if (fake_path[0] == '/') {
		strcpy(result, "/");
	}
	/* It is relative to the current working directory or to a
	 * directory referred by a descriptors, see openat(2) for
	 * details. */
	else {
		struct stat statl;

		/* Format the path to the "virtual" link. */
		if (dir_fd == AT_FDCWD)
			status = sprintf(link, "/proc/%d/cwd", pid);
		else
			status = sprintf(link, "/proc/%d/fd/%d", pid, dir_fd);
		if (status < 0)
			return -EPERM;
		if (status >= sizeof(link))
			return -EPERM;

		/* Read the value of this "virtual" link. */
		status = readlink(link, result, PATH_MAX);
		if (status < 0)
			return -EPERM;
		if (status >= PATH_MAX)
			return -ENAMETOOLONG;
		result[status] = '\0';

		if (dir_fd != AT_FDCWD) {
			/* Ensure it points to a directory. */
			status = stat(result, &statl);
			if (!S_ISDIR(statl.st_mode))
				return -ENOTDIR;
		}

		/* Remove the leading "root" part of the base
		 * (required!). */
		status = detranslate_path(result, 1);
		if (status < 0)
			return status;
	}

	VERBOSE(4, "pid %d: translate(\"%s\" + \"%s\")", pid, result, fake_path);

	/* Canonicalize regarding the new root. */
	status = canonicalize(pid, fake_path, deref_final, result, 0);
	if (status < 0)
		return status;

	/* Don't use the result of the canonicalization if the
	 * translation is delayed, use the origin input path instead. */
	if (use_runner != 0 && tracee != NULL && is_delayed(tracee, fake_path)) {
		if (strlen(fake_path) >= PATH_MAX)
			return -ENAMETOOLONG;
		strcpy(result, fake_path);
		goto end;
	}

	/* Don't prepend the new root to the result of the
	 * canonicalization if it is a binding, instead substitute the
	 * binding location (leading part) with the real path.*/
	if (substitute_binding(BINDING_LOCATION, result) >= 0)
		goto end;

	strcpy(tmp, result);
	status = join_paths(2, result, root, tmp);
	if (status < 0)
		return status;

	/* Small sanity check. */
	if (deref_final != 0 && realpath(result, tmp) != NULL) {
		if (strncmp(tmp, root, root_length) != 0) {
			notice(WARNING, INTERNAL, "tracee %d is out of my control (2)", pid);
			return -EPERM;
		}
	}

end:
	VERBOSE(4, "pid %d:          -> \"%s\"", pid, result);
	return 0;
}

/**
 * Remove the leading "root" part of a previously translated @path. It
 * returns 0 if the leading part was not the "root", otherwise it
 * returns the size in bytes of the updated @path, including the
 * end-of-string terminator. On error it returns -errno.
 */
int detranslate_path(char path[PATH_MAX], int sanity_check)
{
	size_t new_length;

	assert(initialized != 0);

#if BENCHMARK_TRACEE_HANDLING
	return 0;
#endif

	/* Don't try to detranslate relative paths (typically the
	 * target of a relative symbolic link). */
	if (path[0] != '/')
		return 0;

	/* Check if it is a translatable path. */
	if (sanity_check) {
		switch (substitute_binding(BINDING_REAL, path)) {
		case 0:
			return 0;
		case 1:
			return strlen(path) + 1;
		default:
			break;
		}
	}

	/* Ensure the path is within the new root. */
	if (strncmp(path, root, root_length) != 0) {
		if (sanity_check != 0)
			return -EPERM;
		else
			return 0;
	}

	/* Remove the leading part, that is, the "root". */
	assert(strlen(path) >= root_length);
	new_length = strlen(path) - root_length;
	if (new_length != 0) {
		memmove(path, path + root_length, new_length);
		path[new_length] = '\0';
	}
	else {
		/* Special case when path == root. */
		new_length = 1;
		strcpy(path, "/");
	}

	return new_length + 1;
}

typedef int (*foreach_fd_t)(pid_t pid, int fd, char path[PATH_MAX]);

/**
 * Call @callback on each open file descriptors of @pid. It returns
 * the status of the first failure, that is, if @callback returned
 * seomthing lesser than 0, otherwise 0.
 */
static int foreach_fd(pid_t pid, foreach_fd_t callback)
{
	struct dirent *dirent;
	char path[PATH_MAX];
	char proc_fd[32]; /* 32 > sizeof("/proc//fd") + sizeof(#ULONG_MAX) */
	int status;
	DIR *dirp;

	/* Format the path to the "virtual" directory. */
	status = sprintf(proc_fd, "/proc/%d/fd", pid);
	if (status < 0 || status >= sizeof(proc_fd))
		return 0;

	/* Open the virtual directory "/proc/$pid/fd". */
	dirp = opendir(proc_fd);
	if (dirp == NULL)
		return 0;

	while ((dirent = readdir(dirp)) != NULL) {
		/* Read the value of this "virtual" link. */
#ifdef HAVE_READLINKAT
		status = readlinkat(dirfd(dirp), dirent->d_name, path, PATH_MAX);
#else
		char tmp[PATH_MAX];
		if (strlen(proc_fd) + strlen(dirent->d_name) + 1 >= PATH_MAX)
			continue;
		strcpy(tmp, proc_fd);
		strcat(tmp, "/");
		strcat(tmp, dirent->d_name);
		status = readlink(tmp, path, PATH_MAX);
#endif
		if (status < 0 || status >= PATH_MAX)
			continue;
		path[status] = '\0';

		/* Ensure it points to a path (not a socket or somethink like that). */
		if (path[0] != '/')
			continue;

		status = callback(pid, atoi(dirent->d_name), path);
		if (status < 0)
			goto end;
	}
	status = 0;

end:
	closedir(dirp);
	return status;
}

/**
 * Helper for check_fd().
 */
static int check_fd_callback(pid_t pid, int fd, char path[PATH_MAX])
{
	/* XXX TODO: don't warn for files that were open before the attach. */
	if (strncmp(root, path, root_length) != 0) {
		notice(WARNING, INTERNAL, "tracee %d is out of my control (3)", pid);
		notice(WARNING, INTERNAL, "\"%s\" is not inside the new root (\"%s\")", path, root);
		return -pid;
	}
	return 0;
}

/**
 * Check if the file descriptors open by the process @pid point into
 * the new root directory; it returns -@pid if it is not the case,
 * otherwise 0 (or if an ignored error occured).
 */
int check_fd(pid_t pid)
{
	return foreach_fd(pid, check_fd_callback);
}

/**
 * Helper for list_open_fd().
 */
static int list_open_fd_callback(pid_t pid, int fd, char path[PATH_MAX])
{
	VERBOSE(1, "pid %d: access to \"%s\" (fd %d) won't be translated until closed", pid, path, fd);
	return 0;
}

/**
 * Warn for files that are open. It is useful right after PRoot has
 * attached a process.
 */
int list_open_fd(pid_t pid)
{
	return foreach_fd(pid, list_open_fd_callback);
}
