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
#include "attribute.h"

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
	} filtered;

	bool mask_pseudo_files;
	bool discard_argv;

	int input_fd;

	struct {
		FILE *file;
		enum {
			UNSPECIFIED = 0,
			NONE,
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
	uint32_t vpid;
	uint8_t action;
	union {
		struct {
			uint32_t path;
			uint32_t path2;
		};
		struct {
			uint32_t new_vpid;
			uint32_t flags;
		};
		int32_t status;
	} payload;
} PACKED Event;

typedef struct
{
	UT_hash_handle hh;
	char *string;
	size_t index;
} HashedString;

typedef struct {
	Event *events;
	size_t nb_events;
	size_t max_nb_events;
} HistoryChunk;

typedef struct {
	HistoryChunk *history;

	HashedString *strings;
	size_t nb_strings;

	Options *options;
} SharedConfig;

typedef struct {
	SharedConfig *shared;
	bool syscall_creates_path;
	char *argv;
} Config;

#endif /* WIO_H */
