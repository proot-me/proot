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

#define _GNU_SOURCE   /* asprintf(3), */
#include <stdio.h>    /* asprintf(3), */
#include <sys/types.h> /* mkdir(2), */
#include <sys/stat.h> /* lstat(2), mkdir(2), chmod(2), */
#include <fcntl.h>    /* mknod(2), */
#include <unistd.h>   /* getcwd(2), lstat(2), mknod(2), symlink(2), */
#include <stdlib.h>   /* realpath(3), calloc(3), free(3), mkdtemp(3), */
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

/**
 * Print all bindings (verbose purpose).
 */
void print_bindings()
{
	struct binding *binding;

	for (binding = bindings_guest_order; binding != NULL; binding = binding->next) {
		if (compare_paths(binding->host.path, binding->guest.path) == PATHS_ARE_EQUAL)
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
 * Specify the type of the final component during the initialization
 * of a binding.  This variable is defined in bind_path() but it is
 * used later in build_glue_rootfs()...
 */
static mode_t final_type;

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

	/* This function can't be called during the initialization of
	 * a binding.  */
	assert(final_type == 0);

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
 * Insert @binding into the list of @bindings, in a sorted manner so
 * as to make the substitution of nested bindings determistic, ex.:
 *
 *     -b /bin:/foo/bin -b /usr/bin/more:/foo/bin/more
 *
 * Note: "nested" from the @side point-of-view.
 */
static void insort_binding(enum binding_side side, struct binding *binding)
{
	struct binding *bindings;
	struct binding *previous = NULL;
	struct binding *next = NULL;
	struct binding *iterator;

	switch (side) {
	case GUEST_SIDE:
		next = bindings = bindings_guest_order;
		break;

	case HOST_SIDE:
		next = bindings = bindings_host_order;
		break;

	default:
		assert(0);
		return;
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
			return;
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
			return;

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
			return;
		}
	}

	/* Insert this copy in the list.  */
	if (previous != NULL) {
		binding->previous = previous;
		binding->next     = previous->next;
		if (previous->next != NULL)
			previous->next->previous = binding;
		previous->next        = binding;
	}
	else if (next != NULL) {
		binding->next     = next;
		binding->previous = next->previous;
		if (next->previous != NULL)
			next->previous->next = binding;
		next->previous        = binding;

		/* Ensure the head points to the first binding.  */
		if (next == bindings)
			bindings = binding;
	}
	else {
		/* First item in this list.  */
		binding->next     = NULL;
		binding->previous = NULL;
		bindings              = binding;
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
		return;
	}
}

/**
 * c.f. function above.
 */
static void insort_binding2(struct binding *binding)
{
	struct binding *new_binding = malloc(sizeof(struct binding));
	if (new_binding == NULL)
		return;

	insort_binding(GUEST_SIDE, binding);

	/* TODO: do not duplicate the structure, use two sets of links
	 * (one for the guest order, one for the host order).  */
	memcpy(new_binding, binding, sizeof(struct binding));
	insort_binding(HOST_SIDE, new_binding);
}

/**
 * Build in a temporary filesystem the glue between the guest part and
 * the host part of the @binding_path.  This function returns the type
 * of the bound path, otherwise 0 if an error occured.
 *
 * For example, assuming the host path "/opt" is mounted/bound to the
 * guest path "/black/holes/and/revelations", and assuming this path
 * can't be created in the guest rootfs (eg. permission denied), then
 * it is created in a temporary rootfs and all these paths are glued
 * that way:
 *
 *   $GUEST/black/ --> $GLUE/black/
 *                               ./holes
 *                               ./holes/and
 *                               ./holes/and/revelations --> $HOST/opt/
 *
 * This glue allows operations on paths that do not exist in the guest
 * rootfs but that were specified as the guest part of a binding.
 */
mode_t build_glue_rootfs(char binding_path[PATH_MAX], enum finality is_final, bool exist)
{
	size_t offset, delta, size;
	struct binding *binding;
	mode_t type;
	int status;

	/* Create the temporary directory where the "glue" rootfs will
	 * lie.  */
	if (config.glue_rootfs == NULL) {
		status = asprintf(&config.glue_rootfs, "/tmp/proot-%d-XXXXXX", getpid());
		if (status < 0)
			goto end;

		if (mkdtemp(config.glue_rootfs) == NULL) {
			free(config.glue_rootfs);
		end:
			config.glue_rootfs = NULL;
			return 0;
		}
	}

	/* Remember: substitute_binding() was not called on
	 * binding_path, that means the host part of the binding still
	 * points into the guest rootfs.  From the example,
	 * "$GUEST/black".  */
	offset = strlen(config.guest_rootfs);
	if (offset == 1) /* Special case: guest rootfs == "/".  */
		offset = 0;

	/* Check if a new directory entry should be added.  Currently
	 * we just create the path in the guest rootfs but this latter
	 * might be not writable.  A better approach would be the
	 * emulation of getdents(parent(binding_path)).  */
	binding = (struct binding *) get_binding(GUEST_SIDE, binding_path + offset);
	if (binding == NULL) {
		if (!is_final || S_ISDIR(final_type)) {
			(void) mkdir(binding_path, 0777);
			type = S_IFDIR;
		}
		else {
			assert(final_type != 0);
			(void) mknod(binding_path, 0777 | final_type, 0);
			type = final_type;
		}
	}

	/* From the example, substitute "$GUEST/black" with
	 * "$GLUE/black".  */
	delta = strlen(config.glue_rootfs) - offset;
	size  = strlen(binding_path + offset) + 1;
	if (offset + delta + size >= PATH_MAX)
		return 0;

	memmove(binding_path + offset + delta, binding_path + offset, size);
	memcpy(binding_path, config.glue_rootfs, strlen(config.glue_rootfs));

	/* Create this missing path into the glue rootfs.  */
	if (!is_final || S_ISDIR(final_type)) {
		status = mkdir(binding_path, 0777);
		type = S_IFDIR;
	}
	else {
		status = mknod(binding_path, 0777 | final_type, 0);
		type = final_type;
	}
	if (status < 0 && errno != EEXIST) {
		notice(WARNING, SYSTEM, "mkdir/mknod '%s' (binding)", binding_path);
		return 0;
	}

	/* Nothing else to do if the path component already exists in
	 * the guest rootfs or if it is the final component since it
	 * will be pointed to by the binding being initialized (from
	 * the example, "$GUEST/black/holes/and/revelations" ->
	 * "$HOST/opt").  */
	if (exist || is_final || binding != NULL)
		return type;

	/*  From the example, create the binding "/black" ->
	 *  "$GLUE/black".  */
	binding = malloc(sizeof(struct binding));
	if (!binding) {
		notice(WARNING, SYSTEM, "malloc()");
		return 0;
	}

	strcpy(binding->host.path, binding_path);
	binding->host.length = strlen(binding->host.path);

	strcpy(binding->guest.path, binding_path + strlen(config.glue_rootfs));
	binding->guest.length = strlen(binding->guest.path);

	binding->need_substitution = true;

	insort_binding2(binding);

	return type;
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
}

/**
 * Save @path in the list of paths that are bound for the
 * translation mechanism.
 */
void bind_path(const char *host_path, const char *guest_path, bool must_exist)
{
	struct binding *binding = NULL;
	struct stat statl;
	int status;

	binding = calloc(1, sizeof(struct binding));
	if (binding == NULL) {
		notice(WARNING, SYSTEM, "calloc()");
		goto error;
	}

	if (realpath(host_path, binding->host.path) == NULL) {
		if (must_exist)
			notice(WARNING, SYSTEM, "realpath(\"%s\")", host_path);
		goto error;
	}
	binding->host.length = strlen(binding->host.path);

	/* Remember the type of the final component, it will be used
	 * in build_glue_rootfs() later.  */
	status = lstat(binding->host.path, &statl);
	final_type = (status < 0 || S_ISBLK(statl.st_mode) || S_ISCHR(statl.st_mode)
		     ? S_IFREG : statl.st_mode & S_IFMT);

	/* In case binding->guest.path is relative.  */
	if (!getcwd(binding->guest.path, PATH_MAX)) {
		notice(WARNING, SYSTEM, "can't sanitize path (binding) \"%s\", getcwd()", binding->guest.path);
		goto error;
	}

	/* Sanitize the guest path of the binding within the alternate
	   rootfs since it is assumed by substitute_binding().  */
	status = canonicalize(NULL, guest_path ? guest_path : host_path, true, binding->guest.path, 0);
	final_type = 0;
	if (status < 0) {
		notice(WARNING, INTERNAL, "sanitizing the guest path (binding) \"%s\": %s",
			guest_path ? guest_path : host_path, strerror(-status));
		goto error;
	}
	binding->guest.length = strlen(binding->guest.path);

	/* Sanity check.  */
	if (strcmp(binding->guest.path, "/") == 0) {
		notice(WARNING, USER, "can't create a binding in \"/\"");
		goto error;
	}

	binding->need_substitution =
		compare_paths(binding->host.path, binding->guest.path) != PATHS_ARE_EQUAL;

	/* Remove the trailing "/" or "/." as expected by substitute_binding(). */
	if (binding->guest.path[binding->guest.length - 1] == '.') {
		binding->guest.path[binding->guest.length - 2] = '\0';
		binding->guest.length -= 2;
	}
	else if (binding->guest.path[binding->guest.length - 1] == '/') {
		binding->guest.path[binding->guest.length - 1] = '\0';
		binding->guest.length -= 1;
	}

	insort_binding2(binding);

	return;

error:
	if (binding != NULL)
		free(binding);
}
