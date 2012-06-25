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
	struct binding_path host;
	struct binding_path guest;

	bool need_substitution;

	struct binding *previous;
	struct binding *next;
};

static struct binding *bindings_guest_order = NULL;
static struct binding *bindings_host_order = NULL;
static struct binding *expected_bindings = NULL;
static bool initialized = false;

/**
 * Add @binding to the list of expected bindings, used in
 * init_bindings() to feed the list of actual @bindings.
 */
static void expect_binding(struct binding *binding)
{
	binding->next = expected_bindings;
	expected_bindings = binding;
}

/**
 * Save @path in the list of paths that are bound for the
 * translation mechanism.
 */
void bind_path(const char *host_path, const char *guest_path, bool must_exist)
{
	struct binding *binding;
	const char *tmp;

	binding = calloc(1, sizeof(struct binding));
	if (binding == NULL) {
		notice(WARNING, SYSTEM, "calloc()");
		return;
	}

	if (realpath(host_path, binding->host.path) == NULL) {
		if (must_exist)
			notice(WARNING, SYSTEM, "realpath(\"%s\")", host_path);
		goto error;
	}

	binding->host.length = strlen(binding->host.path);

	tmp = guest_path ? guest_path : host_path;
	if (strlen(tmp) >= PATH_MAX) {
		notice(ERROR, INTERNAL, "guest path (binding) \"%s\" is too long", tmp);
		goto error;
	}

	strcpy(binding->guest.path, tmp);

	/* The sanitization of binding->guest is delayed until
	 * init_module_path(). */
	binding->guest.length = 0;

	expect_binding(binding);

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

	for (binding = bindings_guest_order; binding != NULL; binding = binding->next) {
		if (compare_paths2(binding->host.path, binding->host.length,
				   binding->guest.path, binding->guest.length)
			== PATHS_ARE_EQUAL)
			notice(INFO, USER, "binding = %s", binding->host.path);
		else
			notice(INFO, USER, "binding = %s:%s",
				binding->host.path, binding->guest.path);
	}
}

/**
 * Get the binding for the given @path (relatively to the given
 * binding @side).
 */
static const struct binding *get_binding(enum binding_side side, const char path[PATH_MAX])
{
	const struct binding *bindings;
	const struct binding *binding;
	size_t path_length = strlen(path);

	switch (side) {
	case GUEST_SIDE:
		bindings = bindings_guest_order;
		break;

	case HOST_SIDE:
		bindings = bindings_host_order;
		break;

	default:
		assert(0);
		return NULL;
	}

	for (binding = bindings; binding != NULL; binding = binding->next) {
		enum path_comparison comparison;
		const struct binding_path *ref;

		switch (side) {
		case GUEST_SIDE:
			ref = &binding->guest;
			break;

		case HOST_SIDE:
			ref = &binding->host;
			break;

		default:
			assert(0);
			return NULL;
		}

		comparison = compare_paths2(ref->path, ref->length, path, path_length);
		if (   comparison != PATHS_ARE_EQUAL
		    && comparison != PATH1_IS_PREFIX)
			continue;

		/* Avoid false positive when a prefix of the rootfs is
		 * used as an asymmetric binding, ex.:
		 *
		 *     proot -m /usr:/location /usr/local/slackware
		 */
		if (   side == HOST_SIDE
		    && root_length != 1
		    && belongs_to_guestfs(path))
				continue;

		return binding;
	}

	return NULL;
}

/**
 * Get the binding path for the given @path (relatively to the given
 * binding @side).
 */
const char *get_path_binding(enum binding_side side, const char path[PATH_MAX])
{
	const struct binding *binding;

	binding = get_binding(side, path);
	if (!binding)
		return NULL;

	switch (side) {
	case GUEST_SIDE:
		return binding->guest.path;

	case HOST_SIDE:
		return binding->host.path;

	default:
		assert(0);
		return NULL;
	}
}

/**
 * Substitute the guest path (if any) with the host path in @path.
 * This function returns:
 *
 *     * -1 if it is not a binding location
 *
 *     * 0 if it is a binding location but no substitution is needed
 *       ("symetric" binding)
 *
 *     * 1 if it is a binding location and a substitution was performed
 *       ("asymmetric" binding)
 */
int substitute_binding(enum binding_side side, char path[PATH_MAX])
{
	const struct binding_path *reverse_ref;
	const struct binding_path *ref;
	const struct binding *binding;
	size_t path_length;
	size_t new_length;

	if (!initialized)
		return -1;

	binding = get_binding(side, path);
	if (!binding)
		return -1;

	/* Is it a "symetric" binding?  */
	if (!binding->need_substitution)
		return 0;

	path_length = strlen(path);

	switch (side) {
	case GUEST_SIDE:
		ref = &binding->guest;
		reverse_ref = &binding->host;
		break;

	case HOST_SIDE:
		ref = &binding->host;
		reverse_ref = &binding->guest;
		break;

	default:
		assert(0);
		return -1;
	}

	/* Substitute the leading ref' with reverse_ref'.  */
	if (reverse_ref->length == 1) {
		/* Special case when "-b /:/foo".  Substitute
		 * "/foo/bin" with "/bin" not "//bin".  */
		assert(side == GUEST_SIDE);

		new_length = path_length - ref->length;
		if (new_length != 0)
			memmove(path, path + ref->length, new_length);
		else {
			/* Translating "/".  */
			path[0] = '/';
			new_length = 1;
		}
	}
	else if (ref->length == 1) {
		/* Special case when "-b /:/foo". Substitute
		 * "/bin" with "/foo/bin" not "/foobin".  */
		assert(side == HOST_SIDE);

		new_length = reverse_ref->length + path_length;
		if (new_length >= PATH_MAX)
			goto too_long;

		if (path_length > 1) {
			memmove(path + reverse_ref->length, path, path_length);
			memcpy(path, reverse_ref->path, reverse_ref->length);
		}
		else {
			/* Translating "/".  */
			memcpy(path, reverse_ref->path, reverse_ref->length);
			new_length = reverse_ref->length;
		}
	}
	else {
		/* Generic substitution case.  */

		new_length = path_length - ref->length + reverse_ref->length;
		if (new_length >= PATH_MAX)
			goto too_long;

		memmove(path + reverse_ref->length,
			path + ref->length,
			path_length - ref->length);
		memcpy(path, reverse_ref->path, reverse_ref->length);
	}
	path[new_length] = '\0';
	return 1;

too_long:
	return -1;
}

/**
 * Create a "dummy" path up to the canonicalized guest path @c_path,
 * it cheats programs that walk up to it.
 */
static void create_dummy(char c_path[PATH_MAX], const char *real_path)
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

	notice(INFO, USER, "create the guest path (binding) \"%s\"", c_path);
	return;

error:
	notice(WARNING, USER,
		"can't create the guest path (binding) \"%s\": you can still access it *directly*, without seeing it though", c_path);
}

/**
 * Insert @binding into the list of @bindings, in a sorted manner so
 * as to make the substitution of nested bindings determistic, ex.:
 *
 *     -b /bin:/foo/bin -b /usr/bin/more:/foo/bin/more
 *
 * Note: "nested" from the @side point-of-view.
 */
static struct binding *insort_binding(enum binding_side side, const struct binding *binding)
{
	struct binding *bindings;
	struct binding *previous = NULL;
	struct binding *next = NULL;
	struct binding *iterator;
	struct binding *new_binding;

	switch (side) {
	case GUEST_SIDE:
		next = bindings = bindings_guest_order;
		break;

	case HOST_SIDE:
		next = bindings = bindings_host_order;
		break;

	default:
		assert(0);
		return NULL;
	}

	/* Find where it should be added in the list.  */
	for (iterator = bindings; iterator != NULL; iterator = iterator->next) {
		enum path_comparison comparison;
		const struct binding_path *binding_path;
		const struct binding_path *iterator_path;

		switch (side) {
		case GUEST_SIDE:
			binding_path = &binding->guest;
			iterator_path = &iterator->guest;
			break;

		case HOST_SIDE:
			binding_path = &binding->host;
			iterator_path = &iterator->host;
			break;

		default:
			assert(0);
			return NULL;
		}

		comparison = compare_paths2(binding_path->path, binding_path->length,
					    iterator_path->path, iterator_path->length);
		switch (comparison) {
		case PATHS_ARE_EQUAL:
			if (side == HOST_SIDE) {
				previous = iterator;
				break;
			}

			notice(WARNING, USER,
				"both '%s' and '%s' are bound to '%s', only the last binding is active.",
				binding->host.path, iterator->host.path, binding->guest.path);
			return NULL;

		case PATH1_IS_PREFIX:
			/* The new binding contains the iterator.  */
			previous = iterator;
			break;

		case PATH2_IS_PREFIX:
			/* The iterator contains the new binding.
			 * Use the deepest container.  */
			if (next == NULL)
				next = iterator;
			break;

		case PATHS_ARE_NOT_COMPARABLE:
			break;

		default:
			assert(0);
			return NULL;
		}
	}

	/* Work an a copy, the original will be freed by
	 * init_bindings() later.  */
	new_binding = malloc(sizeof(struct binding));
	if (!new_binding) {
		notice(WARNING, SYSTEM, "malloc()");
		return NULL;
	}
	memcpy(new_binding, binding, sizeof(struct binding));

	/* Insert this copy in the list.  */
	if (previous != NULL) {
		new_binding->previous = previous;
		new_binding->next     = previous->next;
		if (previous->next != NULL)
			previous->next->previous = new_binding;
		previous->next        = new_binding;
	}
	else if (next != NULL) {
		new_binding->next     = next;
		new_binding->previous = next->previous;
		if (next->previous != NULL)
			next->previous->next = new_binding;
		next->previous        = new_binding;

		/* Ensure the head points to the first binding.  */
		if (next == bindings)
			bindings = new_binding;
	}
	else {
		/* First item in this list.  */
		new_binding->next     = NULL;
		new_binding->previous = NULL;
		bindings              = new_binding;
	}

	switch (side) {
	case GUEST_SIDE:
		bindings_guest_order = bindings;
		break;

	case HOST_SIDE:
		bindings_host_order = bindings;
		break;

	default:
		assert(0);
		return NULL;
	}

	return new_binding;
}

/**
 * Initialize internal data of the path translator.
 */
void init_bindings()
{
	struct binding *binding;
	struct binding *new_binding;

	/* Now the module is initialized so we can call
	 * canonicalize() safely. */
	for (binding = expected_bindings; binding != NULL; binding = binding->next) {
		char tmp[PATH_MAX];
		int status;

		strcpy(tmp, binding->guest.path);

		/* In case binding->guest.path is relative.  */
		if (!getcwd(binding->guest.path, PATH_MAX)) {
			notice(WARNING, SYSTEM, "can't sanitize path (binding) \"%s\", getcwd()", tmp);
			continue;
		}

		/* Sanitize the guest path of the binding within the
		   alternate rootfs since it is assumed by
		   substitute_binding().  Note the host path is already
		   sanitized in bind_path().  */
		status = canonicalize(0, tmp, true, binding->guest.path, 0);
		if (status < 0) {
			notice(WARNING, INTERNAL, "sanitizing the guest path (binding) \"%s\": %s",
			       tmp, strerror(-status));
			continue;
		}

		if (strcmp(binding->guest.path, "/") == 0) {
			notice(WARNING, USER, "can't create a binding in \"/\"");
			continue;
		}

		binding->guest.length = strlen(binding->guest.path);
		binding->need_substitution =
			compare_paths(binding->host.path,
				binding->guest.path) != PATHS_ARE_EQUAL;

		/* Remove the trailing slash as expected by substitute_binding(). */
		if (binding->guest.path[binding->guest.length - 1] == '/') {
			binding->guest.path[binding->guest.length - 1] = '\0';
			binding->guest.length--;
		}

		new_binding = insort_binding(GUEST_SIDE, binding);
		if (!new_binding)
			continue;

		new_binding = insort_binding(HOST_SIDE, new_binding);
		if (!new_binding)
			continue;

		create_dummy(new_binding->guest.path, new_binding->host.path);
	}

	/* Free the list of expected bindings, it will not be used
	 * anymore.  */
	if (expected_bindings != NULL) {
		struct binding *next;

		for (binding = expected_bindings, next = binding->next;
		     next != NULL;
		     binding = next, next = binding->next)
			free(binding);
	}

	initialized = true;
}
