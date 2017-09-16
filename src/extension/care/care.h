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

#ifndef CARE_H
#define CARE_H

#include <stdbool.h>
#include <sys/queue.h> /* STAILQ_*, */

#include "extension/care/archive.h"

/* Generic item for a STAILQ list.  */
typedef struct item {
	const void *load;
	STAILQ_ENTRY(item) link;
} Item;

typedef STAILQ_HEAD(list, item) List;

/* CARE CLI configuration.  */
typedef struct {
	const char *output;
	char *const *command;

	List *concealed_paths;
	List *revealed_paths;
	List *volatile_paths;
	List *volatile_envars;

	bool ignore_default_config;

	int max_size;
} Options;

/* CARE internal configuration.  */
typedef struct {
	struct Entry *entries;
	struct Entry *dentries;

	char *const *command;
	List *volatile_paths;
	List *volatile_envars;
	List *concealed_accesses;

	const char *prefix;
	const char *output;
	const char *initial_cwd;
	bool ipc_are_volatile;

	Archive *archive;
	int64_t max_size;

	int last_exit_status;

	bool is_ready;
} Care;

extern Item *queue_item(TALLOC_CTX *context, List **list, const char *value);

#endif /* CARE_H */

