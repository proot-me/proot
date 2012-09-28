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
#include <sys/queue.h> /* LIST_*, */
#include <talloc.h>   /* talloc_*, */

#include "path/binding.h"
#include "path/path.h"
#include "path/canon.h"
#include "notice.h"

#define HEAD(tracee, side)  (side == GUEST ? (tracee)->bindings_guest : (tracee)->bindings_host)

#define LIST_FOREACH_(tracee, binding, side)				\
	for (binding = LIST_FIRST(HEAD(tracee, side));			\
	     binding != NULL;						\
	     binding = (side == GUEST					\
		     ? LIST_NEXT(binding, guest_link)			\
		     : LIST_NEXT(binding, host_link)))

#define LIST_INSERT_AFTER_(previous, binding, side) do {	\
	if (side == GUEST)					\
		LIST_INSERT_AFTER(previous, binding, guest_link); \
	else							\
		LIST_INSERT_AFTER(previous, binding, host_link); \
} while (0)

#define LIST_INSERT_BEFORE_(next, binding, side) do {		\
	if (side == GUEST)					\
		LIST_INSERT_BEFORE(next, binding, guest_link);	\
	else							\
		LIST_INSERT_BEFORE(next, binding, host_link);	\
} while (0)

#define LIST_INSERT_HEAD_(tracee, binding, side) do {		\
	if (side == GUEST)					\
		LIST_INSERT_HEAD(HEAD(tracee, side), binding, guest_link); \
	else							\
		LIST_INSERT_HEAD(HEAD(tracee, side), binding, host_link); \
} while (0)

#define IS_LINKED(binding, link)					\
	((binding)->link.le_next != NULL || (binding)->link.le_prev != NULL)

#define LIST_REMOVE_(binding, link) do {		\
	LIST_REMOVE(binding, link);			\
	bzero(&binding->link, sizeof(binding->link));	\
} while (0)

/**
 * Print all bindings (verbose purpose).
 */
static void print_bindings(const Tracee *tracee)
{
	const Binding *binding;

	if (tracee->bindings_guest == NULL)
		return;

	LIST_FOREACH_(tracee, binding, GUEST) {
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

	LIST_FOREACH_(tracee, binding, side) {
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
		    && tracee->root[1] != '\0'
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
	Binding *next = LIST_FIRST(HEAD(tracee, side));

	/* Find where it should be added in the list.  */
	LIST_FOREACH_(tracee, iterator, side) {
		Comparison comparison;
		const Path *binding_path;
		const Path *iterator_path;

		switch (side) {
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
	if (previous != NULL)
		LIST_INSERT_AFTER_(previous, binding, side);
	else if (next != NULL)
		LIST_INSERT_BEFORE_(next, binding, side);
	else
		LIST_INSERT_HEAD_(tracee, binding, side);

	/* Declare tracee->bindings_side as a parent of this
	 * binding.  */
	(void) talloc_reference(HEAD(tracee, side), binding);
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
 * Detach all bindings pointed to by @bindings.
 *
 * Note: this is a Talloc destructor.
 */
static int remove_bindings(Bindings *bindings)
{
	Binding *binding;
	Tracee *tracee;

	/* Each binding is linked to itself (both prev and next), this
	 * ensures no use-after-free will occured in their destructor
	 * (remove_binding()).  */
#define LIST_DETACH_ALL(link) do {				\
	binding = LIST_FIRST(bindings);				\
	while (binding != NULL) {				\
		Binding *next = LIST_NEXT(binding, link);	\
		binding->link.le_prev = &binding;		\
		binding->link.le_next = binding;		\
		binding = next;					\
	}							\
} while (0)

	/* Search which link is used by this list.  */
	tracee = talloc_get_type_abort(talloc_parent(bindings), Tracee);
	if (bindings == tracee->bindings_user)
		LIST_DETACH_ALL(user_link);
	else if (bindings == tracee->bindings_guest)
		LIST_DETACH_ALL(guest_link);
	else if (bindings == tracee->bindings_host)
		LIST_DETACH_ALL(host_link);

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
	if (IS_LINKED(binding, user_link))
		LIST_REMOVE_(binding, user_link);

	if (IS_LINKED(binding, guest_link))
		LIST_REMOVE_(binding, guest_link);

	if (IS_LINKED(binding, host_link))
		LIST_REMOVE_(binding, host_link);

	return 0;
}

/**
 * Allocate a new binding "@host:@guest" and attach it to
 * @tracee->bindings_user.  This function complains about missing
 * @host path only if @must_exist is true.  This function returns the
 * allocated binding on success, NULL on error.
 */
Binding *new_binding(Tracee *tracee, const char *host, const char *guest, bool must_exist)
{
	Binding *binding;

	/* Should be called only from the CLI support.  */
	assert(tracee->pid == 0);

	/* Lasy allocation of the list of bindings specified by the
	 * user.  This list will be used by initialize_bindings().  */
	if (tracee->bindings_user == NULL) {
		tracee->bindings_user = talloc_zero(tracee, Bindings);
		if (tracee->bindings_user == NULL) {
			notice(WARNING, INTERNAL, "talloc_zero() failed");
			return NULL;
		}
		talloc_set_destructor(tracee->bindings_user, remove_bindings);
	}

	/* Allocate an empty binding.  */
	binding = talloc_zero(tracee->bindings_user, Binding);
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

	/* Save the expected guest part, it will be canonicalized by
	 * initialize_bindings().  */
	binding->guest.length = strlen(guest);
	if (binding->guest.length >= PATH_MAX) {
		notice(WARNING, USER, "path to the rootfs is too long (\"%s\")", guest);
		goto error;
	}
	strcpy(binding->guest.path, guest);

	/* The binding to "/" has to be installed before other
	 * bindings since this former is required to canonicalize
	 * these latters (c.f. initialize_binding() for details).  */
	if (compare_paths(binding->guest.path, "/") == PATHS_ARE_EQUAL
	    || LIST_FIRST(tracee->bindings_user) == NULL)
		LIST_INSERT_HEAD(tracee->bindings_user, binding, user_link);
	else
		LIST_INSERT_AFTER(LIST_FIRST(tracee->bindings_user), binding, user_link);
	return binding;

error:
	TALLOC_FREE(binding);
	return NULL;
}

/**
 * Canonicalize the guest part of the given @binding, insert it into
 * @tracee->bindings_guest and @tracee->bindings_host, then remove it
 * from @tracee->bindings_user.  This function returns -1 if an error
 * occured, 0 otherwise.
 */
static int initialize_binding(Tracee *tracee, Binding *binding)
{
	char path[PATH_MAX];
	struct stat statl;
	int status;

	if (compare_paths(binding->guest.path, "/") == PATHS_ARE_EQUAL) {
		if (LIST_FIRST(HEAD(tracee, HOST)) != NULL) {
			notice(WARNING, USER, "explicit binding to \"/\" isn't allowed, "
					    "use the -r option instead");
			goto error;
		}
		strcpy(binding->guest.path, "/");
	}
	else {
		/* When not absolute, assume the guest_path is
		 * relative to the host cwd (eg. ``-b .``).  */
		if (binding->guest.path[0] != '/') {
			char cwd[PATH_MAX];

			if (getcwd(cwd, PATH_MAX) == NULL) {
				notice(WARNING, SYSTEM, "can't sanitize binding \"%s\", getcwd()",
					binding->guest.path);
				goto error;
			}
			join_paths(2, path, cwd, binding->guest.path);
		}
		else
			strcpy(path, binding->guest.path);

		/* Initial state before canonicalization.  */
		strcpy(binding->guest.path, "/");

		/* Remember the type of the final component, it will be used
		 * in build_glue() later.  */
		status = lstat(binding->host.path, &statl);
		tracee->glue_type = (status < 0 || S_ISBLK(statl.st_mode) || S_ISCHR(statl.st_mode)
				? S_IFREG : statl.st_mode & S_IFMT);

		/* Sanitize the guest path of the binding within the alternate
		   rootfs since it is assumed by substitute_binding().  */
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
 * Allocate @tracee->bindings_guest and @tracee->bindings_host, then
 * call initialize_binding() on each binding listed in
 * @tracee->bindings_user.
 */
int initialize_bindings(Tracee *tracee)
{
	Binding *binding;

	/* Sanity checks.  */
	assert(    tracee->bindings_guest == NULL
		&& tracee->bindings_host  == NULL
		&& tracee->bindings_user  != NULL);

	/* Allocate @tracee->bindings_guest and
	 * @tracee->bindings_host.  */
	tracee->bindings_guest = talloc_zero(tracee, Bindings);
	tracee->bindings_host  = talloc_zero(tracee, Bindings);
	if (tracee->bindings_guest == NULL || tracee->bindings_host == NULL) {
		notice(ERROR, INTERNAL, "can't allocate enough memory");
		return -1;
	}

	talloc_set_destructor(tracee->bindings_guest, remove_bindings);
	talloc_set_destructor(tracee->bindings_host, remove_bindings);

	/* Sanity check: the first binding has to be "/".  */
	binding = LIST_FIRST(tracee->bindings_user);
	assert(compare_paths(binding->guest.path, "/") == PATHS_ARE_EQUAL);

	/* Call initialize_binding() on each binding listed in
	 * @tracee->bindings_user.  */
	while (binding != NULL) {
		Binding *next;
		int status;

		next = LIST_NEXT(binding, user_link);

		status = initialize_binding(tracee, binding);
		if (status < 0)
			TALLOC_FREE(binding);
		else
			LIST_REMOVE_(binding, user_link);

		binding = next;
	}

	TALLOC_FREE(tracee->bindings_user);

	if (verbose_level > 0)
		print_bindings(tracee);

	return 0;
}
