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

#include <sys/stat.h> /* lstat(2), */
#include <unistd.h>   /* lstat(2), */
#include <stdlib.h>   /* realpath(3), calloc(3), free(3), */
#include <string.h>   /* string(3),  */
#include <assert.h>   /* assert(3), */
#include <limits.h>   /* PATH_MAX, */
#include <errno.h>    /* errno, E* */

#include "path/binding.h"
#include "path/path.h"
#include "path/canon.h"
#include "notice.h"
#include "config.h"

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
 * Insert @binding into the list of @bindings, in a reversely sorted
 * manner.
 */
static void insort_binding(struct binding *binding)
{
	struct binding *previous = NULL;
	struct binding *next = NULL;

	for (next = bindings; next != NULL; previous = next, next = next->next) {
		if (strcmp(binding->real.path, next->real.path) > 0)
			break;
	}

	if (previous)
		previous->next = binding;
	else
		bindings = binding;

	binding->next = next;
}

/**
 * Save @path in the list of paths that are bound for the
 * translation mechanism.
 */
void bind_path(const char *path, const char *location, bool must_exist)
{
	struct binding *binding;
	const char *tmp;

	binding = calloc(1, sizeof(struct binding));
	if (binding == NULL) {
		notice(WARNING, SYSTEM, "calloc()");
		return;
	}

	if (realpath(path, binding->real.path) == NULL) {
		if (must_exist)
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

	insort_binding(binding);

	return;

error:
	free(binding);
	binding = NULL;
	return;
}

/**
 * Print all bindings (verbose purpose).
 */
void print_bindings()
{
	struct binding *binding;

	for (binding = bindings; binding != NULL; binding = binding->next) {
		if (strcmp(binding->real.path, binding->location.path) == 0)
			notice(INFO, USER, "binding = %s", binding->real.path);
		else
			notice(INFO, USER, "binding = %s:%s",
				binding->real.path, binding->location.path);
	}
}

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
int substitute_binding(int which, char path[PATH_MAX])
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
			if (root_length != 0 /* rootfs != "/" */
			    && belongs_to_guestfs(path))
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

/**
 * Create a "dummy" path up to the canonicalized binding location
 * @c_path, it cheats programs that walk up to it.
 */
static void create_dummy(char c_path[PATH_MAX], const char * real_path)
{
	char t_current_path[PATH_MAX];
	char t_path[PATH_MAX];
	struct stat statl;
	int status;
	int is_final;
	const char *cursor;
	int type;

	/* Determine the type of the element to be bound:
	   1: file
	   0: directory
	*/
	status = stat(real_path, &statl);
	if (status != 0)
		goto error;

	type = (S_ISREG(statl.st_mode));

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

		/* Note that the final component can't always be a
		 * directory, actually its type matters since not only
		 * the entry in the parent directory is important for
		 * some tools like 'find'.  */
		if (is_final) {
			if (type) {
				status = open(t_current_path, O_CREAT, 0766);
				if (status < 0)
					goto error;
				close(status);
			}
			else {
				status = mkdir(t_current_path, 0777);
				if (status < 0 && errno != EEXIST)
					goto error;
			}
		}
		else {
			status = mkdir(t_current_path, 0777);
			if (status < 0 && errno != EEXIST)
				goto error;
		}
	}

	notice(INFO, USER, "create the binding location \"%s\"", c_path);
	return;

error:
	notice(WARNING, USER, "can't create parent directories for \"%s\"", c_path);
}

/**
 * Initialize internal data of the path translator.
 */
void init_bindings()
{
	struct binding *binding;

	/* Now the module is initialized so we can call
	 * canonicalize() safely. */
	for (binding = bindings; binding != NULL; binding = binding->next) {
		char tmp[PATH_MAX];
		int status;

		assert(!binding->sanitized);

		strcpy(tmp, binding->location.path);

		/* In case binding->location.path is relative.  */
		if (!getcwd(binding->location.path, PATH_MAX)) {
			notice(WARNING, SYSTEM, "can't sanitize binding \"%s\"");
			goto error;
		}

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

		/* TODO: use a real path comparison function, for instance
		 * strcmp(3) reports a wrong result with "/foo" and "/foo/."  */
		binding->need_substitution = strcmp(binding->real.path, binding->location.path);
		binding->location.length = strlen(binding->location.path);

		/* Remove the trailing slash as expected by substitute_binding(). */
		if (binding->location.path[binding->location.length - 1] == '/') {
			binding->location.path[binding->location.length - 1] = '\0';
			binding->location.length--;
		}

		create_dummy(binding->location.path, binding->real.path);

		binding->sanitized = 1;
		continue;

	error:
		/* TODO: remove this element from the list instead. */
		binding->sanitized = 0;
	}
}
