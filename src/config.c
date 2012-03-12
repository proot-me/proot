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

#include "config.h"
#include "notice.h"
#include "path/binding.h"
#include "execve/args.h"

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

	print_argv("command", config.command);
	print_argv("qemu", config.qemu);

	if (config.initial_cwd)
		notice(INFO, USER, "initial cwd = %s", config.initial_cwd);

	if (config.kernel_release)
		notice(INFO, USER, "kernel release = %s", config.kernel_release);

	print_bool("allow_unknown_syscalls", config.allow_unknown_syscalls);
	print_bool("disable_aslr", config.disable_aslr);
	print_bool("fake_id0", config.fake_id0);
	print_bool("check_fd", config.check_fd);

	if (config.verbose_level)
		notice(INFO, USER, "verbose level = %d", config.verbose_level);

	print_bindings();
}
