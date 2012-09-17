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

#include <stdbool.h> /* bool, true, false, */
#include <string.h>  /* string(3), */
#include <stdlib.h>  /* ssize_t, */
#include <linux/limits.h> /* ARG_MAX, */
#include <talloc.h>  /* talloc_*, */

#include "config.h"
#include "notice.h"
#include "path/binding.h"

struct config config;

static void print_argv(const char *prompt, char **argv)
{
	int i;
	char string[ARG_MAX] = "";

	if (!argv)
		return;

	void append(const char *post) {
		ssize_t length = sizeof(string) - (strlen(string) + strlen(post));
		if (length <= 0)
			return;
		strncat(string, post, length);
	}

	append(prompt);
	append(" =");
	for (i = 0; argv[i] != NULL; i++) {
		append(" ");
		append(argv[i]);
	}
	string[sizeof(string) - 1] = '\0';

	notice(INFO, USER, "%s", string);
}

static void print_bool(const char *prompt, bool value)
{
	if (value)
		notice(INFO, USER, "%s = true", prompt);
}

void print_config()
{
	notice(INFO, USER, "guest rootfs = %s", config.guest_rootfs);

	if (config.qemu)
		notice(INFO, USER, "host rootfs = %s", HOST_ROOTFS);

	if (config.glue_rootfs)
		notice(INFO, USER, "glue rootfs = %s", config.glue_rootfs);

	print_argv("command", config.command);
	print_argv("qemu", config.qemu);

	if (config.initial_cwd)
		notice(INFO, USER, "initial cwd = %s", config.initial_cwd);

	if (config.kernel_release)
		notice(INFO, USER, "kernel release = %s", config.kernel_release);

	print_bool("fake_id0", config.fake_id0);

	if (config.verbose_level)
		notice(INFO, USER, "verbose level = %d", config.verbose_level);

	print_bindings();
}

void free_config()
{
	if (config.guest_rootfs != NULL)
		TALLOC_FREE(config.guest_rootfs);

	if (config.glue_rootfs != NULL) {
		char *command;
		int status;

		/* Delete only empty files and directories: the files
		 * created by the user inside this glue are kept.  */
		command = talloc_asprintf(NULL, "find %s -empty -delete 2>/dev/null",
					config.glue_rootfs);
		if (command != NULL) {
			status = system(command);
			if (status != 0)
				notice(INFO, USER, "can't delete '%s'", config.glue_rootfs);
			TALLOC_FREE(command);
		}

		TALLOC_FREE(config.glue_rootfs);
	}

	if (config.qemu != NULL)
		TALLOC_FREE(config.qemu);
}
