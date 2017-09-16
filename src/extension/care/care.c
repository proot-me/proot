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

#include <sys/types.h>    /* struct stat, */
#include <sys/stat.h>     /* struct stat, */
#include <unistd.h>       /* lstat(2), */
#include <linux/limits.h> /* PATH_MAX, */
#include <string.h>       /* strlen(3), */
#include <assert.h>       /* assert(3), */
#include <time.h>         /* time(2), localtime(3), */
#include <stddef.h>       /* offsetof(3), */
#include <talloc.h>       /* talloc*, */
#include <uthash.h>       /* ut*, UT*, HASH*, */
#include <sys/queue.h>    /* STAILQ_*, */
#include <inttypes.h>     /* PRI*, */
#include <linux/auxvec.h> /* AT_*, */

#include "extension/care/care.h"
#include "extension/care/final.h"
#include "extension/care/archive.h"
#include "extension/extension.h"
#include "tracee/tracee.h"
#include "tracee/mem.h"
#include "execve/auxv.h"
#include "path/canon.h"
#include "path/path.h"
#include "path/binding.h"
#include "cli/note.h"

/* Make uthash use talloc.  */
#undef  uthash_malloc
#undef  uthash_free
#define uthash_malloc(size) talloc_size(care, size)
#define uthash_free(pointer, size) TALLOC_FREE(pointer)

/* Hash entry.  */
typedef struct Entry {
	UT_hash_handle hh;
	char *path;
} Entry;

/**
 * Add a copy of @value at the end if the given @list.  All the newly
 * talloc'ed elements (duplicated value, item, list head) are attached
 * to the given @context.  This function returns NULL if an error
 * occurred, otherwise the newly talloc'ed item.
 */
Item *queue_item(TALLOC_CTX *context, List **list, const char *value)
{
	Item *item;

	if (*list == NULL) {
		*list = talloc_zero(context, List);
		if (*list == NULL)
			return NULL;

		STAILQ_INIT(*list);
	}

	item = talloc_zero(*list, Item);
	if (item == NULL)
		return NULL;

	item->load = talloc_strdup(item, value);
	if (item->load == NULL)
		return NULL;

	STAILQ_INSERT_TAIL(*list, item, link);
	return item;
}

/**
 * Generate a valid archive @care->output from @care.
 */
static void generate_output_name(const Tracee *tracee, Care *care)
{
	struct tm *splitted_time;
	time_t flat_time;

	flat_time = time(NULL);
	splitted_time = localtime(&flat_time);
	if (splitted_time == NULL) {
		note(tracee, ERROR, INTERNAL,
			"can't generate a valid output name from the current time, "
			"please specify an ouput name explicitly");
		return;
	}

	care->output = talloc_asprintf(care, "care-%02d%02d%02d%02d%02d%02d.%s",
					splitted_time->tm_year - 100, splitted_time->tm_mon + 1,
					splitted_time->tm_mday, splitted_time->tm_hour,
					splitted_time->tm_min, splitted_time->tm_sec,
#if defined(CARE_BINARY_IS_PORTABLE)
					"bin"
#else
					"raw"
#endif
		);
	if (care->output == NULL) {
		note(tracee, ERROR, INTERNAL,
			"can't generate a valid output name from the current time, "
			"please specify an ouput name explicitly");
		return;
	}
}

/**
 * Genereate @extension->config from @options.  This function returns
 * -1 if an error ocurred, otherwise 0.
 */
static int generate_care(Extension *extension, const Options *options)
{
	size_t suffix_length;
	const char *cursor;
	Tracee *tracee;
	Item *item2;
	Item *item;
	Care *care;

	tracee = TRACEE(extension);

	extension->config = talloc_zero(extension, Care);
	if (extension->config == NULL)
		return -1;
	care = extension->config;

	care->command = options->command;
	care->ipc_are_volatile = !options->ignore_default_config;

	if (options->output != NULL)
		care->output = talloc_strdup(care, options->output);
	else
		generate_output_name(tracee, care);
	if (care->output == NULL) {
		note(tracee, WARNING, INTERNAL, "can't get output name");
		return -1;
	}

	care->initial_cwd = talloc_strdup(care, tracee->fs->cwd);
	if (care->initial_cwd == NULL) {
		note(tracee, WARNING, INTERNAL, "can't allocate cwd");
		return -1;
	}

	care->archive = new_archive(care, tracee, care->output, &suffix_length);
	if (care->archive == NULL)
		return -1;

	cursor = strrchr(care->output, '/');
	if (cursor == NULL || strlen(cursor) == 1)
		cursor = care->output;
	else
		cursor++;

	care->prefix = talloc_strndup(care, cursor, strlen(cursor) - suffix_length);
	if (care->prefix == NULL) {
		note(tracee, WARNING, INTERNAL, "can't allocate archive prefix");
		return -1;
	}

	/* Copy & canonicalize volatile paths.  */
	if (options->volatile_paths != NULL) {
		char path[PATH_MAX];
		int status;

		STAILQ_FOREACH(item, options->volatile_paths, link) {
			/* Initial state before canonicalization.  */
			strcpy(path, "/");

			status = canonicalize(tracee, (const char *) item->load, false, path, 0);
			if (status < 0)
				continue;

			/* Sanity check.  */
			if (strcmp(path, "/") == 0) {
				const char *string;
				const char *name;

				name = talloc_get_name(item);
				string = name == NULL || name[0] != '$'
					? talloc_asprintf(tracee->ctx, "'%s'",
							(const char *) item->load)
					: talloc_asprintf(tracee->ctx, "'%s' (%s)",
							(const char *) item->load, name);

				note(tracee, WARNING, USER,
					"path %s was declared volatile but it leads to '/', "
					"as a consequence it will *not* be considered volatile.",
					string);
				continue;
			}

			item2 = queue_item(care, &care->volatile_paths, path);
			if (item2 == NULL)
				continue;

			/* Preserve the non expanded form.  */
			talloc_set_name_const(item2, talloc_get_name(item));

			VERBOSE(tracee, 1, "volatile path: %s",	(const char *) item2->load);
		}
	}

	/* Copy volatile env. variables.  */
	if (options->volatile_envars != NULL) {
		STAILQ_FOREACH(item, options->volatile_envars, link) {
			item2 = queue_item(care, &care->volatile_envars, item->load);
			if (item2 == NULL)
				continue;

			VERBOSE(tracee, 1, "volatile envar: %s", (const char *) item2->load);
		}
	}

	/* Convert the limit from megabytes to bytes, as expected by
	 * handle_host_path().  */
	care->max_size = options->max_size * 1024 * 1024;

	/* handle_host_path() can now be safely used.  */
	care->is_ready = true;

	talloc_set_destructor(care, finalize_care);
	return 0;
}

/**
 * Add @path_ to the list of @care->concealed_accesses.  This function
 * does *not* check for duplicated entries.
 */
static void register_concealed_access(const Tracee *tracee, Care *care, const char *path_)
{
	char path[PATH_MAX];
	size_t length;
	int status;

	length = strlen(path_);
	if (length >= PATH_MAX)
		return;
	memcpy(path, path_, length + 1);

	/* It was a concealed access if, and only if, the path was
	 * part of a asymmetric binding.  */
	status = substitute_binding(tracee, HOST, path);
	if (status != 1)
		return;

	/* Do not register accesses that would not succeed even if the
	 * path was revealed, i.e. the path does not exist at all.  */
	status = access(path, F_OK);
	if (status < 0)
		return;

	queue_item(care, &care->concealed_accesses, path);
	VERBOSE(tracee, 1, "concealed: %s", path);
}

/**
 * Archive @path if needed.
 */
static void handle_host_path(Extension *extension, const char *path)
{
	struct stat statl;
	bool as_dentries;
	char *location;
	Tracee *tracee;
	Entry *entry;
	Care *care;
	int status;

	care = talloc_get_type_abort(extension->config, Care);
	tracee = TRACEE(extension);

	if (!care->is_ready)
		return;

	/* Don't archive if the path was already seen before.
	 * This ensures the rootfs is re-created as it was
	 * before any file creation or modification. */
	HASH_FIND_STR(care->entries, path, entry);
	if (entry != NULL)
		return;

	switch (get_sysnum(tracee, ORIGINAL)) {
	case PR_getdents:
	case PR_getdents64:
		/* Don't archive if the dentry was already seen
		 * before, it would be useless.  */
		HASH_FIND_STR(care->dentries, path, entry);
		if (entry != NULL)
			return;
		as_dentries = true;
		break;

	default:
		as_dentries = false;
		break;
	}

	entry = talloc_zero(care, Entry);
	if (entry == NULL) {
		note(tracee, WARNING, INTERNAL, "can't allocate entry for '%s'", path);
		return;
	}

	entry->path = talloc_strdup(entry, path);
	if (entry->path == NULL) {
		note(tracee, WARNING, INTERNAL, "can't allocate name for '%s'", path);
		return;
	}

	/* Remember this new entry.  */
	if (as_dentries)
		HASH_ADD_KEYPTR(hh, care->dentries, entry->path, strlen(entry->path), entry);
	else
		HASH_ADD_KEYPTR(hh, care->entries, entry->path, strlen(entry->path), entry);

	/* Don't use faccessat(2) here since it would require Linux >=
	 * 2.6.16 and Glibc >= 2.4, whereas CARE is supposed to work
	 * on any Linux 2.6 systems.  */
	status = lstat(path, &statl);
	if (status < 0) {
		register_concealed_access(tracee, care, path);
		return;
	}

	/* FIFOs and Unix domain sockets should be volatile.  */
	if (S_ISFIFO(statl.st_mode) || S_ISSOCK(statl.st_mode)) {
		if (care->ipc_are_volatile) {
			Item *item = queue_item(care, &care->volatile_paths, path);
			if (item != NULL)
				VERBOSE(tracee, 0, "volatile path: %s", path);
			else
				note(tracee, WARNING, USER,
					"can't declare '%s' (fifo or socket) as volatile", path);
			return;
		}
		else
			note(tracee, WARNING, USER,
				"'%1$s' might be explicitely declared volatile (-p %1$s)", path);
	}

	/* Don't archive the content of dentries, this save a lot of
	 * space!  */
	if (as_dentries)
		statl.st_size = 0;

	if (care->volatile_paths != NULL) {
		Item *item;

		STAILQ_FOREACH(item, care->volatile_paths, link) {
			switch (compare_paths(item->load, path)) {
			case PATHS_ARE_EQUAL:
				/* It's a volatile path, archive it as
				 * empty to preserve its dentry.  */
				statl.st_size = 0;
				break;

			case PATH1_IS_PREFIX:
				/* Don't archive it's a sub-part of a
				 * volatile path.  */
				return;

			default:
				continue;
			}
			break;
		}
	}

	if (care->max_size >= 0 && statl.st_size > care->max_size) {
		note(tracee, WARNING, USER,
			"file '%s' is archived with a null size since it is bigger than %"
			PRIi64 "MB, you can specify an alternate limit with the option -m.",
			path, care->max_size / 1024 / 1024);
		statl.st_size = 0;
	}

	/* Format the location within the archive.  */
	location = NULL;
	assert(path[0] == '/');
	location = talloc_asprintf(tracee->ctx, "%s/rootfs%s", care->prefix, path);
	if (location == NULL) {
		note(tracee, WARNING, INTERNAL, "can't allocate location for '%s'", path);
		return;
	}

	status = archive(tracee, care->archive, path, location, &statl);
	if (status == 0)
		VERBOSE(tracee, 1, "archived: %s", path);
}

typedef struct {
	uint32_t d_ino;
	uint32_t next;
	uint16_t size;
	char name[];
} Dirent32;

typedef struct {
	uint64_t d_ino;
	uint64_t next;
	uint16_t size;
	char name[];
} Dirent64;

typedef struct {
	uint64_t inode;
	int64_t  next;
	uint16_t size;
	uint8_t  type;
	char name[];
} NewDirent;

/**
 * Archive all the entries returned by getdents syscalls.
 */
static void handle_getdents(Tracee *tracee, bool is_new_getdents)
{
	char component[PATH_MAX];
	char path[PATH_MAX];
	uint64_t offset;
	int status;

	word_t result;
	word_t buffer;
	word_t fd;

	Dirent32 dirent32;
	Dirent64 dirent64;
	NewDirent new_dirent;

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if ((int) result < 0)
		return;

	fd     = peek_reg(tracee, ORIGINAL, SYSARG_1);
	buffer = peek_reg(tracee, ORIGINAL, SYSARG_2);

	offset = 0;
	while (offset < result) {
		word_t name_offset;
		word_t address;
		size_t size;

		address = buffer + offset;

		if (!is_new_getdents) {
#if defined(ARCH_X86_64)
			const bool is_32bit = is_32on64_mode(tracee);
#else
			const bool is_32bit = true;
#endif
			if (is_32bit) {
				name_offset = offsetof(Dirent32, name);
				status = read_data(tracee, &dirent32, address, sizeof(dirent32));
				size = dirent32.size;
			}
			else {
				name_offset = offsetof(Dirent64, name);
				status = read_data(tracee, &dirent64, address, sizeof(dirent64));
				size = dirent64.size;
			}
		} else {
			name_offset = offsetof(NewDirent, name);
			status = read_data(tracee, &new_dirent, address, sizeof(new_dirent));
			size = new_dirent.size;
		}
		if (status < 0) {
			note(tracee, WARNING, INTERNAL, "can't read dentry");
			break;
		}

		status = read_string(tracee, component, address + name_offset, PATH_MAX);
		if (status < 0 || status >= PATH_MAX) {
			note(tracee, WARNING, INTERNAL, "can't read dentry" );
			goto next;
		}

		/* Archive through the host_path notification. */
		strcpy(path, "/");
		translate_path(tracee, path, fd, component, false);
	next:
		offset += size;
	}

	if (offset != result)
		note(tracee, WARNING, INTERNAL, "dentry table out of sync.");
}

/**
 * Set AT_HWCAP to 0 to ensure no processor specific extensions will
 * be used, for the sake of reproducibility across different CPUs.
 * This function assumes the "argv, envp, auxv" stuff is pointed to by
 * @tracee's stack pointer, as expected right after a successful call
 * to execve(2).
 */
static int adjust_elf_auxv(Tracee *tracee)
{
	ElfAuxVector *vectors;
	ElfAuxVector *vector;
	word_t vectors_address;

	vectors_address = get_elf_aux_vectors_address(tracee);
	if (vectors_address == 0)
		return 0;

	vectors = fetch_elf_aux_vectors(tracee, vectors_address);
	if (vectors == NULL)
		return 0;

	for (vector = vectors; vector->type != AT_NULL; vector++) {
		if (vector->type == AT_HWCAP)
			vector->value = 0;
	}

	push_elf_aux_vectors(tracee, vectors, vectors_address);

	return 0;
}

/* List of syscalls handled by this extensions.  */
static FilteredSysnum filtered_sysnums[] = {
	{ PR_getdents,		FILTER_SYSEXIT },
	{ PR_getdents64,	FILTER_SYSEXIT },
	FILTERED_SYSNUM_END,
};

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int care_callback(Extension *extension, ExtensionEvent event,
		intptr_t data1, intptr_t data2 UNUSED)
{
	Tracee *tracee;

	switch (event) {
	case INITIALIZATION:
		extension->filtered_sysnums = filtered_sysnums;
		return generate_care(extension, (Options *) data1);

	case NEW_STATUS: {
		int status = (int) data1;
		if (WIFEXITED(status)) {
			Care *care = talloc_get_type_abort(extension->config, Care);
			care->last_exit_status = WEXITSTATUS(status);
		}
		return 0;
	}

	case HOST_PATH:
		handle_host_path(extension, (const char *) data1);
		return 0;

	case SYSCALL_EXIT_START:
		tracee = TRACEE(extension);

		switch (get_sysnum(tracee, ORIGINAL)) {
		case PR_getdents:
			handle_getdents(tracee, false);
			break;

		case PR_getdents64:
			handle_getdents(tracee, true);
			break;

		case PR_execve: {
			word_t result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

			/* Note: this can be done only before PRoot pushes the
			 * load script into tracee's stack.  */
			if ((int) result >= 0)
				adjust_elf_auxv(tracee);
			break;
		}

		default:
			break;
		}
		return 0;

	default:
		return 0;
	}
}
