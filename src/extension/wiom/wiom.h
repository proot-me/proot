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
#include <sys/queue.h>	/* SIMPLEQ, */

#include "arch.h"

typedef struct item {
	void *load;
	SIMPLEQ_ENTRY(item) link;
} Item;

typedef SIMPLEQ_HEAD(list, item) List;

typedef struct {
	struct {
		bool successful_actions;		/* (TODO: switchable)	*/
		bool unsuccessful_actions;		/* (TODO)		*/

		bool path_traversal;			/* (TODO: switchable)	*/
		bool path_type;				/* (TODO)		*/
		bool path_content_usage;		/* (TODO: switchable)	*/
		bool path_metadata_usage;		/* (TODO: switchable)	*/

		bool process_usage; /* clone, execve */	/* (TODO: switchable)	*/
	} record;

	bool coalesce_events;			/* (TODO: WIP)		*/
	List *masked_paths;			/* (TODO: WIP)		*/
	List *unmasked_paths;			/* (TODO: WIP)		*/

	struct {
		const char *path;			/* (TODO)		*/
		enum {
			TEXT_EVERYTHING,		/* (TODO)		*/
			TEXT_IO_FILES,			/* (TODO)		*/
			KCONFIG_FS_USAGE,		/* (TODO)		*/
			KCONFIG_PROCESS_TREE,		/* (TODO)		*/
			KCONFIG_FS_DEPENDENCIES,	/* (TODO)		*/
			GMAKE_FS_DEPENDENCIES,		/* (TODO)		*/
		} format;
	} output;
} Options;

typedef enum {
	TRAVERSES,
	CREATES,
	DELETES,
	GETS_METADATA_OF,
	SETS_METADATA_OF,
	GETS_CONTENT_OF,
	SETS_CONTENT_OF,
	EXECUTES,
	MOVES,
	IS_CLONED,
	HAS_EXITED,
} Action;

typedef struct {
	pid_t pid;
	Action action;
	union {
		struct {
			const char *path;
			const char *path2;
		};
		struct {
			pid_t pid;
			bool thread;
		};
		word_t status;
	} load;
} Event;

typedef struct
{
	UT_hash_handle hh;
	char *string;
} HashedString;

typedef struct {
	Event *history;
	HashedString *strings;

	/* TODO: coalescing */

	bool open_creates_path;

	Options *options;
} Config;

#endif /* WIO_H */
