/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
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

#include <assert.h>     /* assert(3), */
#include <talloc.h>     /* talloc_*, */
#include <sys/queue.h>  /* LIST_*, */
#include <strings.h>    /* bzero(3), */

#include "extension/extension.h"
#include "cli/note.h"
#include "build.h"

#include "compat.h"
#include "extension/portmap/portmap.h"

/**
 * Remove an @extension from its tracee's list, then send it the
 * "REMOVED" event.
 *
 * Note: this is a Talloc destructor.
 */
static int remove_extension(Extension *extension)
{
	LIST_REMOVE(extension, link);
	extension->callback(extension, REMOVED, 0, 0);

	bzero(extension, sizeof(Extension));
	return 0;
}

/**
 * Allocate a new extension for the given @callback then attach it to
 * its @tracee.  This function returns NULL on error, otherwise the
 * new extension.
 */
static Extension *new_extension(Tracee *tracee, extension_callback_t callback)
{
	Extension *extension;

	/* Lazy allocation of the list head. */
	if (tracee->extensions == NULL) {
		tracee->extensions = talloc_zero(tracee, Extensions);
		if (tracee->extensions == NULL)
			return NULL;
	}

	/* Allocate a new extension. */
	extension = talloc_zero(tracee->extensions, Extension);
	if (extension == NULL)
		return NULL;
	extension->callback = callback;

	/* Attach it to its tracee. */
	LIST_INSERT_HEAD(tracee->extensions, extension, link);
	talloc_set_destructor(extension, remove_extension);

	return extension;
}

/**
 * Retrieve from @tracee->extensions the extension for the given
 * @callback.
 */
Extension *get_extension(Tracee *tracee, extension_callback_t callback)
{
	Extension *extension;

	if (tracee->extensions == NULL)
		return NULL;

	LIST_FOREACH(extension, tracee->extensions, link) {
		if (extension->callback == callback)
			return extension;
	}

	return NULL;
}

/**
 * Initialize a new extension for the given @callback then attach it
 * to its @tracee.  The parameter @cli is its argument that was passed
 * to the command-line interface.  This function return -1 if an error
 * occurred, otherwise 0.
 */
int initialize_extension(Tracee *tracee, extension_callback_t callback, const char *cli)
{
	Extension *extension;
	int status;

	extension = new_extension(tracee, callback);
	if (extension == NULL) {
		note(tracee, WARNING, INTERNAL, "can't create a new extension");
		return -1;
	}

	/* Remove the new extension if its initialized has failed.  */
	status = extension->callback(extension, INITIALIZATION, (intptr_t) cli, 0);
	if (status < 0) {
		TALLOC_FREE(extension);
		return status;
	}

	return 0;
}

/**
 * Rebuild a new list of extensions for this @child from its @parent.
 * The inheritance model is controlled by the @parent.
 */
void inherit_extensions(Tracee *child, Tracee *parent, word_t clone_flags)
{
	Extension *parent_extension;
	Extension *child_extension;
	int status;

	if (parent->extensions == NULL)
		return;

	/* Sanity check.  */
	assert(child->extensions == NULL || clone_flags == CLONE_RECONF);

	LIST_FOREACH(parent_extension, parent->extensions, link) {
		/* Ask the parent how this extension is
		 * inheritable.  */
		status = parent_extension->callback(parent_extension, INHERIT_PARENT,
						(intptr_t)child, clone_flags);

		/* Not inheritable.  */
		if (status < 0)
			continue;

		/* Inheritable...  */
		child_extension = new_extension(child, parent_extension->callback);
		if (child_extension == NULL) {
			note(parent, WARNING, INTERNAL,
				"can't create a new extension for pid %d", child->pid);
			continue;
		}

		if (status == 0) {
			/* ... with a shared config or ...  */
			child_extension->config =
				talloc_reference(child_extension, parent_extension->config);
		}
		else {
			/* ... with another inheritance model.  */
			child_extension->callback(child_extension, INHERIT_CHILD,
						(intptr_t)parent_extension, clone_flags);
		}
	}
}
