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

#include <sys/types.h> /* mkdir(2), */
#include <sys/stat.h> /* mkdir(2), */
#include <fcntl.h>    /* mknod(2), */
#include <unistd.h>   /* mknod(2), */
#include <stdlib.h>   /* mkdtemp(3), */
#include <string.h>   /* string(3),  */
#include <assert.h>   /* assert(3), */
#include <limits.h>   /* PATH_MAX, */
#include <errno.h>    /* errno, E* */
#include <talloc.h>   /* talloc_*, */

#include "path/binding.h"
#include "path/path.h"
#include "notice.h"

#include "compat.h"

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
			notice(NULL, INFO, USER, "can't delete '%s'", path);
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
	bool belongs_to_gluefs;
	Comparison comparison;
	Binding *binding;
	mode_t type;
	mode_t mode;
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

	comparison = compare_paths(tracee->glue, host_path);
	belongs_to_gluefs = (comparison == PATHS_ARE_EQUAL || comparison == PATH1_IS_PREFIX);

	/* If it's not a final component then it is a directory.  I definitely
	 * hate how the potential type of the final component is propagated
	 * from initialize_binding() down to here, sadly there's no elegant way
	 * to know its type at this stage.  */
	if (is_final) {
		type = tracee->glue_type;
		mode = (belongs_to_gluefs ? 0777 : 0);
	}
	else {
		type = S_IFDIR;
		mode = 0777;
	}

	if (getenv("PROOT_DONT_POLLUTE_ROOTFS") != NULL && !belongs_to_gluefs)
		goto create_binding;

	/* Try to create this component into the "guest" or "glue"
	 * rootfs (depending if there were a glue previously).  */
	if (S_ISDIR(type))
		status = mkdir(host_path, mode);
	else /* S_IFREG, S_IFCHR, S_IFBLK, S_IFIFO or S_IFSOCK.  */
		status = mknod(host_path, mode | type, 0);

	/* Nothing else to do if the path already exists or if it is
	 * the final component since it will be pointed to by the
	 * binding being initialized (from the example,
	 * "$GUEST/black/holes/and/revelations" -> "$HOST/opt").  */
	if (status >= 0 || errno == EEXIST || is_final)
		return type;

	/* mkdir/mknod are supposed to always succeed in
	 * tracee->glue.  */
	if (belongs_to_gluefs) {
		notice(tracee, WARNING, SYSTEM, "mkdir/mknod");
		return 0;
	}

create_binding:

	/* From the example, create the binding "/black" ->
	 * "$GLUE/black".  */
	binding = talloc_zero(tracee->glue, Binding);
	if (binding == NULL)
		return 0;

	strcpy(binding->host.path, tracee->glue);
	strcpy(binding->guest.path, guest_path);

	binding->host.length = strlen(binding->host.path);
	binding->guest.length = strlen(binding->guest.path);

	insort_binding2(tracee, binding);

	/* TODO: emulation of getdents(parent(guest_path)) to finalize
	 * the glue, "black" in getdents("/") from the example.  */

	return type;
}
