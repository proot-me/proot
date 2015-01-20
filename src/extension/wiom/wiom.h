/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2014 STMicroelectronics
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

#ifndef WIO_H
#define WIO_H

#include <stdbool.h>	/* bool, */
#include <uthash.h>	/* ut_hash_handle, */
#include <sys/queue.h>	/* STAILQ, */

#include "arch.h"

typedef struct filtered_path {
	char *path;
	bool masked;
	STAILQ_ENTRY(filtered_path) link;
} FilteredPath;

typedef STAILQ_HEAD(filtered_paths, filtered_path) FilteredPaths;

typedef struct {
	struct {
		unsigned long actions;
		FilteredPaths *paths;

		bool success;	/* (TODO) */
		bool failure;	/* (TODO) */
	} filtered;

	int input_fd;

	struct {
		FILE *file;
		enum {
			NONE = 0,
			DUMP,
			TRACE,
			FS_STATE,
			KCONFIG_FS_USAGE,		/* (TODO) */
			KCONFIG_PROCESS_TREE,		/* (TODO) */
			KCONFIG_FS_DEPENDENCIES,	/* (TODO) */
			GMAKE_FS_DEPENDENCIES,		/* (TODO) */
		} format;
	} output;
} Options;

typedef enum {
	#define ACTION(name) name,
	#include "extension/wiom/actions.list"
	#undef ACTION
} Action;

#define GET_FILTERED_ACTION_BIT(options, action) ((1 << (action)) & (options)->filtered.actions)
#define SET_FILTERED_ACTION_BIT(options, action) ((options)->filtered.actions |= (1 << (action)))
#define UNSET_FILTERED_ACTION_BIT(options, action) ((options)->filtered.actions &= ~(1 << (action)))

typedef struct {
	pid_t pid;
	Action action;
	union {
		struct {
			const char *path;
			const char *path2;
		};
		struct {
			pid_t new_pid;
			word_t flags;
		};
		word_t status;
	} payload;
} Event;

typedef struct
{
	UT_hash_handle hh;
	char *string;
} HashedString;

typedef struct {
	Event **history;
	HashedString *strings;
	Options *options;
} SharedConfig;

typedef struct {
	SharedConfig *shared;
	bool syscall_creates_path;
} Config;

#endif /* WIO_H */
