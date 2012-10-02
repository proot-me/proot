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
#include <stdlib.h>   /* realpath(3), mkdtemp(3), */
#include <string.h>   /* string(3),  */
#include <strings.h>  /* bzero(3), */
#include <assert.h>   /* assert(3), */
#include <limits.h>   /* PATH_MAX, */
#include <errno.h>    /* errno, E* */
#include <stdio.h>    /* sprintf(3), */
#include <sys/queue.h> /* CIRCLEQ_*, */
#include <talloc.h>   /* talloc_*, */

#include "path/binding.h"
#include "path/path.h"
#include "path/canon.h"
#include "notice.h"

#define HEAD(tracee, side)						\
	(side == GUEST							\
		? (tracee)->bindings.guest				\
		: (side == HOST						\
			? (tracee)->bindings.host			\
			: (tracee)->bindings.pending))

#define NEXT(binding, side)						\
	(side == GUEST							\
		? CIRCLEQ_NEXT(binding, link.guest)			\
		: (side == HOST						\
			? CIRCLEQ_NEXT(binding, link.host)		\
			: CIRCLEQ_NEXT(binding, link.pending)))

#define CIRCLEQ_FOREACH_(tracee, binding, side)				\
	for (binding = CIRCLEQ_FIRST(HEAD(tracee, side));		\
	     binding != (void *) HEAD(tracee, side);			\
	     binding = NEXT(binding, side))

#define CIRCLEQ_INSERT_AFTER_(tracee, previous, binding, side) do {	\
	switch (side) {							\
	case GUEST: CIRCLEQ_INSERT_AFTER(HEAD(tracee, side), previous, binding, link.guest);   break; \
	case HOST:  CIRCLEQ_INSERT_AFTER(HEAD(tracee, side), previous, binding, link.host);    break; \
	default:    CIRCLEQ_INSERT_AFTER(HEAD(tracee, side), previous, binding, link.pending); break; \
	}								\
} while (0)

#define CIRCLEQ_INSERT_BEFORE_(tracee, next, binding, side) do {	\
	switch (side) {							\
	case GUEST: CIRCLEQ_INSERT_BEFORE(HEAD(tracee, side), next, binding, link.guest);   break; \
	case HOST:  CIRCLEQ_INSERT_BEFORE(HEAD(tracee, side), next, binding, link.host);    break; \
	default:    CIRCLEQ_INSERT_BEFORE(HEAD(tracee, side), next, binding, link.pending); break; \
	}								\
} while (0)

#define CIRCLEQ_INSERT_HEAD_(tracee, binding, side) do {		\
	switch (side) {							\
	case GUEST: CIRCLEQ_INSERT_HEAD(HEAD(tracee, side), binding, link.guest);	  break; \
	case HOST:  CIRCLEQ_INSERT_HEAD(HEAD(tracee, side), binding, link.host);    break; \
	default:    CIRCLEQ_INSERT_HEAD(HEAD(tracee, side), binding, link.pending); break; \
	}								\
} while (0)

#define IS_LINKED(binding, link)					\
	((binding)->link.cqe_next != NULL || (binding)->link.cqe_prev != NULL)

#define CIRCLEQ_REMOVE_(tracee, binding, name) \
	CIRCLEQ_REMOVE((tracee)->bindings.name, binding, link.name);

/**
 * Print all bindings (verbose purpose).
 */
static void print_bindings(const Tracee *tracee)
{
	const Binding *binding;

	if (tracee->bindings.guest == NULL)
		return;

	CIRCLEQ_FOREACH_(tracee, binding, GUEST) {
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
static const Binding *get_binding(Tracee *tracee, Side side, const char path[PATH_MAX])
{
	const Binding *binding;
	size_t path_length = strlen(path);

	/* Sanity checks.  */
	assert(path != NULL && path[0] == '/');

	CIRCLEQ_FOREACH_(tracee, binding, side) {
		Comparison comparison;
		const Path *ref;

		switch (side) {
		case GUEST:
			ref = &binding->guest;
			break;

		case HOST:
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
		if (   side == HOST
		    && compare_paths(get_root(tracee), "/") != PATHS_ARE_EQUAL
		    && belongs_to_guestfs(tracee, path))
				continue;

		return binding;
	}

	return NULL;
}

/**
 * Get the binding path for the given @path (relatively to the given
 * binding @side).
 */
const char *get_path_binding(Tracee *tracee, Side side, const char path[PATH_MAX])
{
	const Binding *binding;

	binding = get_binding(tracee, side, path);
	if (!binding)
		return NULL;

	switch (side) {
	case GUEST:
		return binding->guest.path;

	case HOST:
		return binding->host.path;

	default:
		assert(0);
		return NULL;
	}
}

/**
 * Return the path to the guest rootfs for the given @tracee, from the
 * host point-of-view obviously.  Depending on whether
 * initialize_bindings() was called or not, the path is retrieved from
 * the "bindings.guest" list or from the "bindings.pending" list,
 * respectively.
 */
const char *get_root(const Tracee* tracee)
{
	const Binding *binding;

	if (tracee->bindings.guest == NULL) {
		if (tracee->bindings.pending == NULL || CIRCLEQ_EMPTY(tracee->bindings.pending))
			return NULL;

		binding = CIRCLEQ_LAST(tracee->bindings.pending);
		if (compare_paths(binding->guest.path, "/") != PATHS_ARE_EQUAL)
			return NULL;

		return binding->host.path;
	}

	assert(!CIRCLEQ_EMPTY(tracee->bindings.guest));

	binding = CIRCLEQ_LAST(tracee->bindings.guest);

	assert(strcmp(binding->guest.path, "/") == 0);

	return binding->host.path;
}

/**
 * Substitute the guest path (if any) with the host path in @path.
 * This function returns:
 *
 *     * -errno if an error occured
 *
 *     * 0 if it is a binding location but no substitution is needed
 *       ("symetric" binding)
 *
 *     * 1 if it is a binding location and a substitution was performed
 *       ("asymmetric" binding)
 */
int substitute_binding(Tracee *tracee, Side side, char path[PATH_MAX])
{
	const Path *reverse_ref;
	const Path *ref;
	const Binding *binding;
	size_t path_length;
	size_t new_length;

	binding = get_binding(tracee, side, path);
	if (!binding)
		return -ENOENT;

	/* Is it a "symetric" binding?  */
	if (!binding->need_substitution)
		return 0;

	path_length = strlen(path);

	switch (side) {
	case GUEST:
		ref = &binding->guest;
		reverse_ref = &binding->host;
		break;

	case HOST:
		ref = &binding->host;
		reverse_ref = &binding->guest;
		break;

	default:
		assert(0);
		return -EACCES;
	}

	/* Substitute the leading ref' with reverse_ref'.  */
	if (reverse_ref->length == 1) {
		/* Special case when "-b /:/foo".  Substitute
		 * "/foo/bin" with "/bin" not "//bin".  */

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

		new_length = reverse_ref->length + path_length;
		if (new_length >= PATH_MAX)
			return -ENAMETOOLONG;

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
			return -ENAMETOOLONG;

		memmove(path + reverse_ref->length,
			path + ref->length,
			path_length - ref->length);
		memcpy(path, reverse_ref->path, reverse_ref->length);
	}
	path[new_length] = '\0';
	return 1;
}

/**
 * Insert @binding into the list of @bindings, in a sorted manner so
 * as to make the substitution of nested bindings determistic, ex.:
 *
 *     -b /bin:/foo/bin -b /usr/bin/more:/foo/bin/more
 *
 * Note: "nested" from the @side point-of-view.
 */
static void insort_binding(const Tracee *tracee, Side side, Binding *binding)
{
	Binding *iterator;
	Binding *previous = NULL;
	Binding *next = CIRCLEQ_FIRST(HEAD(tracee, side));

	/* Find where it should be added in the list.  */
	CIRCLEQ_FOREACH_(tracee, iterator, side) {
		Comparison comparison;
		const Path *binding_path;
		const Path *iterator_path;

		switch (side) {
		case PENDING:
		case GUEST:
			binding_path = &binding->guest;
			iterator_path = &iterator->guest;
			break;

		case HOST:
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
			if (side == HOST) {
				previous = iterator;
				break;
			}

			notice(WARNING, USER,
				"both '%s' and '%s' are bound to '%s', "
				"only the last binding is active.",
				iterator->host.path, binding->host.path, binding->guest.path);

			/* Replace this iterator with the new binding.  */
			CIRCLEQ_INSERT_AFTER_(tracee, iterator, binding, side);
			TALLOC_FREE(iterator);
			return;

		case PATH1_IS_PREFIX:
			/* The new binding contains the iterator.  */
			previous = iterator;
			break;

		case PATH2_IS_PREFIX:
			/* The iterator contains the new binding.
			 * Use the deepest container.  */
			if (next == (void *) HEAD(tracee, side))
				next = iterator;
			break;

		case PATHS_ARE_NOT_COMPARABLE:
			break;

		default:
			assert(0);
			return;
		}
	}

	/* Insert this binding in the list.  */
	if (previous != NULL)
		CIRCLEQ_INSERT_AFTER_(tracee, previous, binding, side);
	else if (next != (void *) HEAD(tracee, side))
		CIRCLEQ_INSERT_BEFORE_(tracee, next, binding, side);
	else
		CIRCLEQ_INSERT_HEAD_(tracee, binding, side);

	/* Declare tracee->bindings.side as a parent of this
	 * binding.  */
	if (side != PENDING)
		(void) talloc_reference(HEAD(tracee, side), binding);
	else
		assert(talloc_parent(binding) == HEAD(tracee, side));
}

/**
 * c.f. function above.
 */
static void insort_binding2(Tracee *tracee, Binding *binding)
{
	insort_binding(tracee, GUEST, binding);
	insort_binding(tracee, HOST, binding);
}

/**
 * Delete only empty files and directories from the glue: the files
 * created by the user inside this glue are left.
 *
 * Note: this is a Talloc desctructor.
 */
static int remove_glue(char *path)
{
	char *command;

	command = talloc_asprintf(NULL, "find %s -empty -delete 2>/dev/null", path);
	if (command != NULL) {
		int status;

		status = system(command);
		if (status != 0)
			notice(INFO, USER, "can't delete '%s'", path);
	}

	TALLOC_FREE(command);

	return 0;
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
mode_t build_glue(Tracee *tracee, const char *guest_path, char host_path[PATH_MAX], Finality is_final)
{
	Comparison comparison;
	Binding *binding;
	mode_t type;
	int status;

	assert(tracee->glue_type != 0);

	/* Create the temporary directory where the "glue" rootfs will
	 * lie.  */
	if (tracee->glue == NULL) {
		tracee->glue = talloc_asprintf(tracee, "/tmp/proot-%d-XXXXXX", getpid());
		if (tracee->glue == NULL)
			return 0;
		talloc_set_name_const(tracee->glue, "$glue");

		if (mkdtemp(tracee->glue) == NULL) {
			TALLOC_FREE(tracee->glue);
			return 0;
		}

		talloc_set_destructor(tracee->glue, remove_glue);
	}

	/* If it's not a final component then it is a directory.  I
	 * definitively hate how the type of the final component is
	 * propagated from bind_path() down to here, sadly there's no
	 * elegant way to know its type at this stage.  */
	if (is_final)
		type = tracee->glue_type;
	else
		type = S_IFDIR;

	/* Try to create this component into the "guest" or "glue"
	 * rootfs (depending if there were a glue previously).  */
	if (S_ISDIR(type))
		status = mkdir(host_path, 0777);
	else
		status = mknod(host_path, 0777 | type, 0);

	/* Nothing else to do if the path already exists or if it is
	 * the final component since it will be pointed to by the
	 * binding being initialized (from the example,
	 * "$GUEST/black/holes/and/revelations" -> "$HOST/opt").  */
	if (status >= 0 || errno == EEXIST || is_final)
		return type;

	/* mkdir/mknod are supposed to always succeed in
	 * tracee->glue.  */
	comparison = compare_paths(tracee->glue, host_path);
	if (   comparison == PATHS_ARE_EQUAL
	    || comparison == PATH1_IS_PREFIX) {
		notice(WARNING, SYSTEM, "mkdir/mknod");
		return 0;
	}

	/* From the example, create the binding "/black" ->
	 * "$GLUE/black".  */
	binding = talloc_zero(tracee->glue, Binding);
	if (!binding) {
		notice(WARNING, INTERNAL, "talloc_zero() failed");
		return 0;
	}

	strcpy(binding->host.path, tracee->glue);
	binding->host.length = strlen(binding->host.path);

	strcpy(binding->guest.path, guest_path);
	binding->guest.length = strlen(binding->guest.path);

	binding->need_substitution = true;

	insort_binding2(tracee, binding);

	/* TODO: emulation of getdents(parent(guest_path)) to finalize
	 * the glue, "black" in getdent("/") from the example.  */

	return type;
}

/**
 * Free all bindings from @bindings.
 *
 * Note: this is a Talloc destructor.
 */
static int remove_bindings(Bindings *bindings)
{
	Binding *binding;
	Tracee *tracee;

	/* Free all bindings from the @link list.  */
#define CIRCLEQ_REMOVE_ALL(link) do {				\
	binding = CIRCLEQ_FIRST(bindings);			\
	while (binding != (void *) bindings) {			\
		Binding *next = CIRCLEQ_NEXT(binding, link);	\
		talloc_set_destructor(binding, NULL);		\
		talloc_unlink(bindings, binding);		\
		binding = next;					\
	}							\
} while (0)

	/* Search which link is used by this list.  */
	tracee = talloc_get_type_abort(talloc_parent(bindings), Tracee);
	if (bindings == tracee->bindings.pending)
		CIRCLEQ_REMOVE_ALL(link.pending);
	else if (bindings == tracee->bindings.guest)
		CIRCLEQ_REMOVE_ALL(link.guest);
	else if (bindings == tracee->bindings.host)
		CIRCLEQ_REMOVE_ALL(link.host);

	bzero(bindings, sizeof(Bindings));

	return 0;
}

/**
 * Remove @binding from all the list of bindings it belongs to.
 *
 * Note: this is a Talloc destructor.
 */
static int remove_binding(Binding *binding)
{
	Tracee *tracee = TRACEE(binding);

	if (IS_LINKED(binding, link.pending))
		CIRCLEQ_REMOVE_(tracee, binding, pending);

	if (IS_LINKED(binding, link.guest))
		CIRCLEQ_REMOVE_(tracee, binding, guest);

	if (IS_LINKED(binding, link.host))
		CIRCLEQ_REMOVE_(tracee, binding, host);

	return 0;
}

/**
 * Allocate a new binding "@host:@guest" and attach it to
 * @tracee->bindings.pending.  This function complains about missing
 * @host path only if @must_exist is true.  This function returns the
 * allocated binding on success, NULL on error.
 */
Binding *new_binding(Tracee *tracee, const char *host, const char *guest, bool must_exist)
{
	Binding *binding;
	char base[PATH_MAX];
	int status;

	/* Should be called only from the CLI support.  */
	assert(tracee->pid == 0);

	/* Lasy allocation of the list of bindings specified by the
	 * user.  This list will be used by initialize_bindings().  */
	if (tracee->bindings.pending == NULL) {
		tracee->bindings.pending = talloc_zero(tracee, Bindings);
		if (tracee->bindings.pending == NULL) {
			notice(WARNING, INTERNAL, "talloc_zero() failed");
			return NULL;
		}
		CIRCLEQ_INIT(tracee->bindings.pending);
		talloc_set_destructor(tracee->bindings.pending, remove_bindings);
	}

	/* Allocate an empty binding.  */
	binding = talloc_zero(tracee->bindings.pending, Binding);
	if (binding == NULL) {
		notice(WARNING, INTERNAL, "talloc_zero() failed");
		return NULL;
	}
	talloc_set_destructor(binding, remove_binding);

	/* Expand environment variables like $HOME.  */
	if (host[0] == '$' && getenv(&host[1]))
		host = getenv(&host[1]);

	/* Canonicalize the host part of the binding, as expected by
	 * get_binding().  */
	if (realpath(host, binding->host.path) == NULL) {
		if (must_exist)
			notice(WARNING, SYSTEM, "realpath(\"%s\")", host);
		goto error;
	}
	binding->host.length = strlen(binding->host.path);

	/* Symetric binding?  */
	guest = guest ?: host;

	/* When not absolute, assume the guest path is relative to the
	 * current working directory, as with ``-b .`` for instance.  */
	if (guest[0] != '/') {
		if (getcwd(base, PATH_MAX) == NULL) {
			notice(WARNING, SYSTEM, "can't sanitize binding \"%s\", getcwd()",
				binding->guest.path);
			goto error;
		}
	}
	else
		strcpy(base, "/");

	status = join_paths(2, binding->guest.path, base, guest);
	if (status < 0) {
		notice(WARNING, SYSTEM, "can't sanitize binding \"%s\"", binding->guest.path);
		goto error;
	}
	binding->guest.length = strlen(binding->guest.path);

	/* Keep the list of bindings specified by the user ordered,
	 * for the sake of consistency.  For instance binding to "/"
	 * has to be the last in the list.  */
	insort_binding(tracee, PENDING, binding);

	return binding;

error:
	TALLOC_FREE(binding);
	return NULL;
}

/**
 * Canonicalize the guest part of the given @binding, insert it into
 * @tracee->bindings.guest and @tracee->bindings.host, then remove it
 * from @tracee->bindings.pending.  This function returns -1 if an
 * error occured, 0 otherwise.
 */
static int initialize_binding(Tracee *tracee, Binding *binding)
{
	char path[PATH_MAX];
	struct stat statl;
	int status;

	/* All bindings but "/" must be canonicalized.  The exception
	 * for "/" is required to bootstrap the canonicalization.  */
	if (compare_paths(binding->guest.path, "/") != PATHS_ARE_EQUAL) {
		/* Initial state before canonicalization.  */
		strcpy(path, binding->guest.path);
		strcpy(binding->guest.path, "/");

		/* Remember the type of the final component, it will
		 * be used in build_glue() later.  */
		status = lstat(binding->host.path, &statl);
		tracee->glue_type = (status < 0 || S_ISBLK(statl.st_mode) || S_ISCHR(statl.st_mode)
				? S_IFREG : statl.st_mode & S_IFMT);

		/* Sanitize the guest path of the binding within the
		   alternate rootfs since it is assumed by
		   substitute_binding().  */
		status = canonicalize(tracee, path, true, binding->guest.path, 0);
		if (status < 0) {
			notice(WARNING, INTERNAL, "sanitizing the guest path (binding) \"%s\": %s",
				path, strerror(-status));
			goto error;
		}

		/* Disable definitively the creation of the glue for
		 * this binding.  */
		tracee->glue_type = 0;
	}

	binding->guest.length = strlen(binding->guest.path);

	binding->need_substitution =
		compare_paths(binding->host.path, binding->guest.path) != PATHS_ARE_EQUAL;

	/* Remove the trailing "/" or "/." as expected by substitute_binding(). */
	if (binding->guest.path[binding->guest.length - 1] == '.') {
		binding->guest.path[binding->guest.length - 2] = '\0';
		binding->guest.length -= 2;
	}
	else if (binding->guest.path[binding->guest.length - 1] == '/'
		&& binding->guest.length > 1 /* Special case for "/".  */) {
		binding->guest.path[binding->guest.length - 1] = '\0';
		binding->guest.length -= 1;
	}

	insort_binding2(tracee, binding);
	return 0;

error:
	return -1;
}

/**
 * Allocate @tracee->bindings.guest and @tracee->bindings.host, then
 * call initialize_binding() on each binding listed in
 * @tracee->bindings.pending.
 */
int initialize_bindings(Tracee *tracee)
{
	Binding *binding;

	/* Sanity checks.  */
	assert(    tracee->bindings.guest == NULL
		&& tracee->bindings.host  == NULL
		&& tracee->bindings.pending != NULL);

	/* Allocate @tracee->bindings.guest and
	 * @tracee->bindings.host.  */
	tracee->bindings.guest = talloc_zero(tracee, Bindings);
	tracee->bindings.host  = talloc_zero(tracee, Bindings);
	if (tracee->bindings.guest == NULL || tracee->bindings.host == NULL) {
		notice(ERROR, INTERNAL, "can't allocate enough memory");
		return -1;
	}

	CIRCLEQ_INIT(tracee->bindings.guest);
	CIRCLEQ_INIT(tracee->bindings.host);

	talloc_set_destructor(tracee->bindings.guest, remove_bindings);
	talloc_set_destructor(tracee->bindings.host, remove_bindings);

	/* The binding to "/" has to be installed before other
	 * bindings since this former is required to canonicalize
	 * these latters.  */
	binding = CIRCLEQ_LAST(tracee->bindings.pending);
	assert(compare_paths(binding->guest.path, "/") == PATHS_ARE_EQUAL);

	/* Call initialize_binding() on each pending binding in
	 * reverse order: the last binding "/" is used to bootstrap
	 * the canonicalization.  */
	while (binding != (void *) tracee->bindings.pending) {
		Binding *previous;
		int status;

		previous = CIRCLEQ_PREV(binding, link.pending);

		status = initialize_binding(tracee, binding);
		if (status < 0)
			TALLOC_FREE(binding);
		else
			CIRCLEQ_REMOVE_(tracee, binding, pending);

		binding = previous;
	}

	TALLOC_FREE(tracee->bindings.pending);

	if (verbose_level > 0)
		print_bindings(tracee);

	return 0;
}
