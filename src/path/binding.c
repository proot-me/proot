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

#include <sys/types.h> /* mkdir(2), */
#include <sys/stat.h> /* lstat(2), mkdir(2), chmod(2), */
#include <fcntl.h>    /* mknod(2), */
#include <unistd.h>   /* getcwd(2), lstat(2), mknod(2), symlink(2), */
#include <stdlib.h>   /* realpath(3), calloc(3), free(3), mktemp(3), */
#include <string.h>   /* string(3),  */
#include <assert.h>   /* assert(3), */
#include <limits.h>   /* PATH_MAX, */
#include <errno.h>    /* errno, E* */
#include <stdio.h>    /* sprintf(3), */

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
static struct binding *user_bindings = NULL;
static bool initialized = false;

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

	/* Add this binding to the list of user bindings, this latter
	 * is converted in init_bindings().  */
	binding->next = user_bindings;
	user_bindings = binding;

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
		    && config.guest_rootfs[1] != '\0'
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
 * Create a "fake" filesystem branch for components of
 * binding->guest.path that are not in the guest rootfs (for instance
 * "-b /etc/motd:/this/does/not/exists").  This function returns NULL
 * on error, otherwse it returns a binding used as a glue between the
 * parent of the first missing component and the "fake" filesystem
 * branch.
 */
static struct binding *create_missing_components(const struct binding *binding)
{
	struct binding *new_binding = NULL;
	enum finality is_final;
	char parent[PATH_MAX];
	char path[PATH_MAX];
	const char *cursor;
	int status;

	/* Remember that "binding->guest.path" is canonicalized and
	 * that substitute_binding() will not work until this module
	 * is initialized.  That's why we can safely concatenate
	 * "root" and "binding->guest.path" here.  */
	status = join_paths(2, path, config.guest_rootfs, binding->guest.path);
	if (status < 0)
		goto error;

	/* Start after the guest root since this latter is known to
	 * exist, obviously.  */
	cursor = path + strlen(config.guest_rootfs);
	strcpy(parent, config.guest_rootfs);

	is_final = NOT_FINAL;
	while (!is_final) {
		char component[NAME_MAX];
		char tmp[PATH_MAX];
		struct stat statl;
		size_t size;

		status = next_component(component, &cursor);
		if (status < 0)
			goto error;
		is_final = status;

		/* Copy up to the current component.  */
		size = cursor - path - 1;
		assert(size < PATH_MAX);

		memcpy(tmp, path, size);
		tmp[size] = '\0';

		/* Nothing to do if this path exists.  */
		status = lstat(tmp, &statl);
		if (status == 0) {
			strcpy(parent, tmp);
			continue;
		}

		/* The path does not exist, so let's create the
		 * missing components in a dedicated bindings.  */
		if (new_binding == NULL) {
			/* End of the story.  */
			if (is_final)
				break;

			/* This shall be freed by the caller after
			 * being "insorted".  */
			new_binding = malloc(sizeof(struct binding));
			if (!new_binding) {
				notice(WARNING, SYSTEM, "malloc()");
				goto error;
			}

			/* Create a binding that connect the parent of
			 * the first missing component to a fake
			 * filesystem branch.  The purpose of this
			 * branch is to avoid a black hole.  */

			status = detranslate_path(NULL, tmp, NULL);
			if (status < 0)
				goto error;

			strcpy(new_binding->guest.path, tmp);
			new_binding->guest.length = strlen(new_binding->guest.path);

			sprintf(new_binding->host.path, "/tmp/proot-%d-XXXXXX", getpid());
			new_binding->host.length = strlen(new_binding->host.path);

			new_binding->need_substitution = true;

			/* Get a temporary name for the fake
			 * filesystem branch.  */
			mktemp(new_binding->host.path);
			if (new_binding->host.path[0] == '\0')
				goto error;

			/* Compute the component to be created.  */
			strcpy(tmp, new_binding->host.path);
		}
		else {
			/* Compute the component to be created.  */
			status = join_paths(2, tmp, parent, component);
			if (status < 0)
				goto error;
		}

		if (is_final) {
			/* Compute the kind of component.  Note:
			 * devices are created as regular files
			 * because it would require privileges
			 * otherwise.  */
			status = lstat(binding->host.path, &statl);
			if (status < 0 || S_ISCHR(statl.st_mode) || S_ISBLK(statl.st_mode))
				statl.st_mode = S_IFREG;
		}
		else
			/* A non-final component has to be directory.  */
			statl.st_mode = S_IFDIR;

		/* Create the component.  */
		switch (statl.st_mode & S_IFMT) {
		case S_IFDIR:
			status = mkdir(tmp, 0777);
			break;
					
		case S_IFLNK:
			status = symlink("nowhere", tmp);
			break;

		default:
			status = mknod(tmp, 0777 | statl.st_mode, 0);
			break;
		}
		if (status < 0) {
			notice(WARNING, SYSTEM, "mkdir/symlink/mknod '%d'", tmp);
			goto error;
		}

		/* Ensure the parent is not writable anymore.  */
		(void) chmod(parent, 0555);
		strcpy(parent, tmp);
	}
	return new_binding;

error:
	notice(WARNING, USER,
		"can't create the guest path (binding) \"%s\": you can still access it directly", binding->guest.path);
	return NULL;
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
 * Free all the @bindings of the given list.
 */
static void free_bindings2(struct binding *bindings)
{
	while (bindings != NULL) {
		struct binding *next =  bindings->next;
		free(bindings);
		bindings = next;
	}
}

/**
 * Free all the bindings.
 */
void free_bindings()
{
	free_bindings2(bindings_guest_order);
	bindings_guest_order = NULL;

	free_bindings2(bindings_host_order);
	bindings_host_order = NULL;

	free_bindings2(user_bindings);
	user_bindings = NULL;
}

/**
 * Initialize internal data of the path translator.
 */
void init_bindings()
{
	struct binding *binding;
	struct binding *dirent_binding;

	/* Now the module is initialized so we can call
	 * canonicalize() safely. */
	for (binding = user_bindings; binding != NULL; binding = binding->next) {
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

		(void) insort_binding(GUEST_SIDE, binding);
		(void) insort_binding(HOST_SIDE, binding);

		/* Ensure there's no black hole in the file-system hierarchy.  */
		dirent_binding = create_missing_components(binding);
		if (dirent_binding != NULL) {
			(void) insort_binding(GUEST_SIDE, dirent_binding);
			(void) insort_binding(HOST_SIDE, dirent_binding);
			free(dirent_binding);
		}
	}

	/* Free the list of user bindings, they will not be used
	 * anymore.  */
	free_bindings2(user_bindings);
	user_bindings = NULL;

	initialized = true;
}
