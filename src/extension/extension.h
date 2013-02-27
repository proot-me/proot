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

#ifndef EXTENSION_H
#define EXTENSION_H

#include <sys/queue.h> /* LIST_, */
#include <stdint.h>    /* intptr_t, */
#include <stdbool.h>   /* bool, */

#include "extension/extension.h"
#include "tracee/tracee.h"

/* List of possible events.  */
typedef enum {
	/* A guest path passed as an argument of the current syscall
	 * is about to be translated: "(char *) data1" is the base for
	 * "(char *) data2" -- the guest path -- if this latter is
	 * relative.  If the extension returns > 0, then PRoot skips
	 * its own handling.  If the extension returns < 0, then PRoot
	 * reports this errno as-is.  */
	GUEST_PATH,

	/* A canonicalized host path is being accessed during the
	 * translation of a guest path: "(char *) data1" is the
	 * canonicalized host path and "(bool) data2" is true if it is
	 * the final path.  Note that several host paths are accessed
	 * for a given guest path since PRoot has to walk along all
	 * parent directories and symlinks in order to translate it.
	 * If the extension returns < 0, then PRoot reports this errno
	 * as-is.  */
	HOST_PATH,

	/* The tracee enters a syscall, and PRoot hasn't do anything
	 * yet.  If the extension returns > 0, then PRoot skips its
	 * own handling.  If the extension returns < 0, then PRoot
	 * reports this errno to the tracee.  */
	SYSCALL_ENTER_START,

	/* The tracee enters a syscall, and PRoot has already handled
	 * it.  If the extension returns < 0, then PRoot reports this
	 * errno to the tracee.  */
	SYSCALL_ENTER_END,

	/* The tracee exits a syscall, and PRoot hasn't do anything
	 * yet.  If the extension returns > 0, then PRoot skips its
	 * own handling.  If the extension returns < 0, then PRoot
	 * reports this errno to the tracee.  */
	SYSCALL_EXIT_START,

	/* The tracee exits a syscall, and PRoot has already handled
	 * it.  If the extension returns < 0, then PRoot reports this
	 * errno to the tracee.  */
	SYSCALL_EXIT_END,

	/* The tracee is stopped either because of a syscall or a
	 * signal: "(int) data1" is its new status as reported by
	 * waitpid(2).  If the extension returns != 0, then PRoot
	 * skips its own handling.  */
	NEW_STATUS,

	/* Ask how this extension is inheritable: "(Tracee *) data1" is the
	 * child tracee and "(bool) data2" specifies if it's a
	 * sub-reconfiguration.  The meaning of the returned value is:
	 *
	 *   < 0 : not inheritable
	 *  == 0 : inheritable + shared configuration.
	 *   > 0 : inheritable + call INHERIT_CHILD.  */
	INHERIT_PARENT,

	/* Control the inheritance: "(Extension *) data1" is the extension of
	 * the parent and "(bool) data2" specifies if it's a
	 * sub-reconfiguration.  For instance the extension in the child could
	 * create a new configuration depending on the parent's
	 * configuration.  */
	INHERIT_CHILD,

	/* Initialize the extension: "(const char *) data1" is its
	 * argument that was passed to the command-line interface.  If
	 * the extension returns < 0, then PRoot removed it.  */
	INITIALIZATION,

	/* The extension is not attached to its tracee anymore
	 * (destructor).  */
	REMOVED,

	/* Print the current configuration of the extension.  See
	 * print_config() as an example.  */
	PRINT_CONFIG,

	/* Print the usage of the extension: "(bool) data1" is true
	 * for a detailed usage.  See print_usage() as an example.  */
	PRINT_USAGE,
} ExtensionEvent;

struct extension;
typedef int (*extension_callback_t)(struct extension *extension, ExtensionEvent event,
				intptr_t data1, intptr_t data2);

typedef struct extension {
	/* Function to be called when any event occured.  */
	extension_callback_t callback;

	/* A chunk of memory allocated by any talloc functions.
	 * Mainly useful to store a configuration.  */
	TALLOC_CTX *config;

	/* Link to the next and previous extensions.  Note the order
	 * is *never* garantee.  */
	LIST_ENTRY(extension) link;
} Extension;

typedef LIST_HEAD(extensions, extension) Extensions;

extern int initialize_extension(Tracee *tracee, extension_callback_t callback, const char *cli);
extern void inherit_extensions(Tracee *child, Tracee *parent, bool sub_reconf);

/**
 * Notify all extensions of @tracee that the given @event occured.
 * See ExtensionEvent for the meaning of @data1 and @data2.
 */
static inline int notify_extensions(Tracee *tracee, ExtensionEvent event,
				intptr_t data1, intptr_t data2)
{
	Extension *extension;

	if (tracee->extensions == NULL)
		return 0;

	LIST_FOREACH(extension, tracee->extensions, link) {
		int status = extension->callback(extension, event, data1, data2);
		if (status != 0)
			return status;
	}

	return 0;
}

/* Built-in extensions.  */
extern int kompat_callback(Extension *extension, ExtensionEvent event, intptr_t d1, intptr_t d2);
extern int fake_id0_callback(Extension *extension, ExtensionEvent event, intptr_t d1, intptr_t d2);

#endif /* EXTENSION_H */
