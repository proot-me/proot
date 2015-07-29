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

#include <sys/types.h>  /* lstat(2), lseek(2), */
#include <sys/stat.h>   /* lstat(2), lseek(2), fchmod(2), */
#include <unistd.h>     /* access(2), lstat(2), close(2), read(2), */
#include <errno.h>      /* E*, */
#include <assert.h>     /* assert(3), */
#include <talloc.h>     /* talloc*, */
#include <sys/mman.h>   /* PROT_*, */
#include <string.h>     /* strlen(3), strcpy(3), */
#include <stdlib.h>     /* getenv(3), */
#include <stdio.h>      /* fwrite(3), */
#include <assert.h>     /* assert(3), */

#include "execve/execve.h"
#include "execve/shebang.h"
#include "execve/aoxp.h"
#include "execve/ldso.h"
#include "execve/elf.h"
#include "path/path.h"
#include "path/temp.h"
#include "path/binding.h"
#include "tracee/tracee.h"
#include "syscall/syscall.h"
#include "syscall/sysnum.h"
#include "arch.h"
#include "cli/note.h"

#define P(a) PROGRAM_FIELD(load_info->elf_header, *program_header, a)

/**
 * Add @program_header (type PT_LOAD) to @load_info->mappings.  This
 * function returns -errno if an error occured, otherwise it returns
 * 0.
 */
static int add_mapping(const Tracee *tracee UNUSED, LoadInfo *load_info,
		const ProgramHeader *program_header)
{
	size_t index;
	word_t start_address;
	word_t end_address;
	static word_t page_size = 0;
	static word_t page_mask = 0;

	if (page_size == 0) {
		page_size = sysconf(_SC_PAGE_SIZE);
		if ((int) page_size <= 0)
			page_size = 0x1000;
		page_mask = ~(page_size - 1);
	}

	if (load_info->mappings == NULL)
		index = 0;
	else
		index = talloc_array_length(load_info->mappings);

	load_info->mappings = talloc_realloc(load_info, load_info->mappings, Mapping, index + 1);
	if (load_info->mappings == NULL)
		return -ENOMEM;

	start_address = P(vaddr) & page_mask;
	end_address   = (P(vaddr) + P(filesz) + page_size) & page_mask;

	load_info->mappings[index].fd     = -1; /* Unknown yet.  */
	load_info->mappings[index].offset = P(offset) & page_mask;
	load_info->mappings[index].addr   = start_address;
	load_info->mappings[index].length = end_address - start_address;
	load_info->mappings[index].flags  = MAP_PRIVATE | MAP_FIXED;
	load_info->mappings[index].prot   =  ( (P(flags) & PF_R ? PROT_READ  : 0)
					| (P(flags) & PF_W ? PROT_WRITE : 0)
					| (P(flags) & PF_X ? PROT_EXEC  : 0));

	/* "If the segment's memory size p_memsz is larger than the
	 * file size p_filesz, the "extra" bytes are defined to hold
	 * the value 0 and to follow the segment's initialized area."
	 * -- man 7 elf.  */
	if (P(memsz) > P(filesz)) {
		/* How many extra bytes in the current page?  */
		load_info->mappings[index].clear_length = end_address - P(vaddr) - P(filesz);

		/* Create new pages for the remaining extra bytes.  */
		start_address = end_address;
		end_address   = (P(vaddr) + P(memsz) + page_size) & page_mask;
		if (end_address > start_address) {
			index++;
			load_info->mappings = talloc_realloc(load_info, load_info->mappings,
							Mapping, index + 1);
			if (load_info->mappings == NULL)
				return -ENOMEM;

			load_info->mappings[index].fd     = -1;  /* Anonymous.  */
			load_info->mappings[index].offset =  0;
			load_info->mappings[index].addr   = start_address;
			load_info->mappings[index].length = end_address - start_address;
			load_info->mappings[index].clear_length = 0;
			load_info->mappings[index].flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
			load_info->mappings[index].prot   = load_info->mappings[index - 1].prot;
		}
	}
	else
		load_info->mappings[index].clear_length = 0;

	return 0;
}

/**
 * Translate @user_path into @host_path and check if this latter exists, is
 * executable and is a regular file.  This function returns -errno if
 * an error occured, 0 otherwise.
 */
int translate_and_check_exec(Tracee *tracee, char host_path[PATH_MAX], const char *user_path)
{
	struct stat statl;
	int status;

	if (user_path[0] == '\0')
		return -ENOEXEC;

	status = translate_path(tracee, host_path, AT_FDCWD, user_path, true);
	if (status < 0)
		return status;

	status = access(host_path, F_OK);
	if (status < 0)
		return -ENOENT;

	status = access(host_path, X_OK);
	if (status < 0)
		return -EACCES;

	status = lstat(host_path, &statl);
	if (status < 0)
		return -EPERM;

	return 0;
}

/**
 * Add @program_header (type PT_INTERP) to @load_info->interp.  This
 * function returns -errno if an error occured, otherwise it returns
 * 0.
 */
static int add_interp(Tracee *tracee, int fd, LoadInfo *load_info,
		const ProgramHeader *program_header)
{
	char host_path[PATH_MAX];
	char *user_path;
	int status;

	/* Only one PT_INTERP segment is allowed.  */
	if (load_info->interp != NULL)
		return -EINVAL;

	load_info->interp = talloc_zero(load_info, LoadInfo);
	if (load_info->interp == NULL)
		return -ENOMEM;

	user_path = talloc_size(tracee->ctx, P(filesz) + 1);
	if (user_path == NULL)
		return -ENOMEM;

	/* Remember pread(2) doesn't change the
	 * current position in the file.  */
	status = pread(fd, user_path, P(filesz), P(offset));
	if ((size_t) status != P(filesz)) /* Unexpected size.  */
		status = -EACCES;
	if (status < 0)
		return status;

	user_path[P(filesz)] = '\0';

	/* When a QEMU command was specified:
	 *
	 * - if it's a foreign binary we are reading the ELF
	 *   interpreter of QEMU instead.
	 *
	 * - if it's a host binary, we are reading its ELF
	 *   interpreter.
	 *
	 * In both case, it lies in "/host-rootfs" from a guest
	 * point-of-view.  */
	if (tracee->qemu != NULL && user_path[0] == '/') {
		user_path = talloc_asprintf(tracee->ctx, "%s%s", HOST_ROOTFS, user_path);
		if (user_path == NULL)
			return -ENOMEM;
	}

	status = translate_and_check_exec(tracee, host_path, user_path);
	if (status < 0)
		return status;

	load_info->interp->host_path = talloc_strdup(load_info->interp, host_path);
	if (load_info->interp->host_path == NULL)
		return -ENOMEM;

	load_info->interp->user_path = talloc_strdup(load_info->interp, user_path);
	if (load_info->interp->user_path == NULL)
		return -ENOMEM;

	return 0;
}

#undef P

struct add_load_info_data {
	LoadInfo *load_info;
	Tracee *tracee;
	int fd;
};

/**
 * This function is a program header iterator.  It invokes
 * add_mapping() or add_interp(), according to the type of
 * @program_header.  This function returns -errno if an error
 * occurred, otherwise 0.
 */
static int add_load_info(const ElfHeader *elf_header,
			const ProgramHeader *program_header, void *data_)
{
	struct add_load_info_data *data = data_;
	int status;

	switch (PROGRAM_FIELD(*elf_header, *program_header, type)) {
	case PT_LOAD:
		status = add_mapping(data->tracee, data->load_info, program_header);
		if (status < 0)
			return status;
		break;

	case PT_INTERP:
		status = add_interp(data->tracee, data->fd, data->load_info, program_header);
		if (status < 0)
			return status;
		break;

	case PT_GNU_STACK:
		data->load_info->needs_executable_stack |=
			((PROGRAM_FIELD(*elf_header, *program_header, flags) & PF_X) != 0);
		break;

	default:
		break;
	}

	return 0;
}

/**
 * Extract the load info from @load->host_path.  This function returns
 * -errno if an error occured, otherwise it returns 0.
 */
static int extract_load_info(Tracee *tracee, LoadInfo *load_info)
{
	struct add_load_info_data data;
	int fd = -1;
	int status;

	assert(load_info != NULL);
	assert(load_info->host_path != NULL);

	fd = open_elf(load_info->host_path, &load_info->elf_header);
	if (fd < 0)
		return fd;

	/* Sanity check.  */
	switch (ELF_FIELD(load_info->elf_header, type)) {
	case ET_EXEC:
	case ET_DYN:
		break;

	default:
		status = -EINVAL;
		goto end;
	}

	data.load_info = load_info;
	data.tracee    = tracee;
	data.fd        = fd;

	status = iterate_program_headers(tracee, fd, &load_info->elf_header, add_load_info, &data);
end:
	if (fd >= 0)
		close(fd);

	return status;
}

/**
 * Add @load_base to each adresses of @load_info.
 */
static void add_load_base(LoadInfo *load_info, word_t load_base)
{
	size_t nb_mappings;
	size_t i;

	nb_mappings = talloc_array_length(load_info->mappings);
	for (i = 0; i < nb_mappings; i++)
		load_info->mappings[i].addr += load_base;

	if (IS_CLASS64(load_info->elf_header))
		load_info->elf_header.class64.e_entry += load_base;
	else
		load_info->elf_header.class32.e_entry += load_base;
}

/**
 * Compute the final load address for each position independant
 * objects of @tracee.
 *
 * TODO: support for ASLR.
 */
static void compute_load_addresses(Tracee *tracee)
{
	if (IS_POSITION_INDENPENDANT(tracee->load_info->elf_header)
	    && tracee->load_info->mappings[0].addr == 0) {
#if defined(HAS_LOADER_32BIT)
		if (IS_CLASS32(tracee->load_info->elf_header))
			add_load_base(tracee->load_info, EXEC_PIC_ADDRESS_32);
		else
#endif
		add_load_base(tracee->load_info, EXEC_PIC_ADDRESS);
	}

	/* Nothing more to do?  */
	if (tracee->load_info->interp == NULL)
		return;

	if (IS_POSITION_INDENPENDANT(tracee->load_info->interp->elf_header)
	    && tracee->load_info->interp->mappings[0].addr == 0) {
#if defined(HAS_LOADER_32BIT)
		if (IS_CLASS32(tracee->load_info->elf_header))
			add_load_base(tracee->load_info->interp, INTERP_PIC_ADDRESS_32);
		else
#endif
		add_load_base(tracee->load_info->interp, INTERP_PIC_ADDRESS);
	}
}

/**
 * Expand in argv[] and envp[] the runner for @user_path, if needed.
 * This function returns -errno if an error occurred, otherwise 0.  On
 * success, both @host_path and @user_path point to the program to
 * execute (respectively from host and guest point-of-views), and both
 * @tracee's argv[] (pointed to by SYSARG_2) @tracee's envp[] (pointed
 * to by SYSARG_3) are correctly updated.
 */
static int expand_runner(Tracee* tracee, char host_path[PATH_MAX], char user_path[PATH_MAX])
{
	ArrayOfXPointers *envp;
	char *argv0;
	int status;

	/* Execution of host programs when QEMU is in use relies on
	 * LD_ environment variables.  */
	status = fetch_array_of_xpointers(tracee, &envp, SYSARG_3, 0);
	if (status < 0)
		return status;

	/* Environment variables should be compared with the "name"
	 * part of the "name=value" string format.  */
	envp->compare_xpointee = (compare_xpointee_t) compare_xpointee_env;

	/* No need to adjust argv[] if it's a host binary (a.k.a
	 * mixed-mode).  */
	if (!is_host_elf(tracee, host_path)) {
		ArrayOfXPointers *argv;
		size_t nb_qemu_args;
		size_t i;

		status = fetch_array_of_xpointers(tracee, &argv, SYSARG_2, 0);
		if (status < 0)
			return status;

		status = read_xpointee_as_string(argv, 0, &argv0);
		if (status < 0)
			return status;

		/* Assuming PRoot was invoked this way:
		 *
		 *     proot -q 'qemu-arm -cpu cortex-a9' ...
		 *
		 * a call to:
		 *
		 *     execve("/bin/true", { "true", NULL }, ...)
		 *
		 * becomes:
		 *
		 *     execve("/usr/bin/qemu",
		 *           { "qemu", "-cpu", "cortex-a9", "-0", "true", "/bin/true", NULL }, ...)
		 */

		nb_qemu_args = talloc_array_length(tracee->qemu) - 1;
		status = resize_array_of_xpointers(argv, 1, nb_qemu_args + 2);
		if (status < 0)
			return status;

		for (i = 0; i < nb_qemu_args; i++) {
			status = write_xpointee(argv, i, tracee->qemu[i]);
			if (status < 0)
				return status;
		}

		status = write_xpointees(argv, i, 3, "-0", argv0, user_path);
		if (status < 0)
			return status;

		/* Ensure LD_ features should not be applied to QEMU
		 * iteself.  */
		status = ldso_env_passthru(tracee, envp, argv, "-E", "-U", i);
		if (status < 0)
			return status;

		status = push_array_of_xpointers(argv, SYSARG_2);
		if (status < 0)
			return status;

		/* Launch the runner in lieu of the initial
		 * program. */
		assert(strlen(tracee->qemu[0]) + strlen(HOST_ROOTFS) < PATH_MAX);
		assert(tracee->qemu[0][0] == '/');

		strcpy(host_path, tracee->qemu[0]);

		strcpy(user_path, HOST_ROOTFS);
		strcat(user_path, host_path);
	}

	/* Provide information to the host dynamic linker to find host
	 * libraries (remember the guest root file-system contains
	 * libraries for the guest architecture only).  */
	status = rebuild_host_ldso_paths(tracee, host_path, envp);
	if (status < 0)
		return status;

	status = push_array_of_xpointers(envp, SYSARG_3);
	if (status < 0)
		return status;

	return 0;
}

extern unsigned char _binary_loader_exe_start;
extern unsigned char _binary_loader_exe_end;

extern unsigned char WEAK _binary_loader_m32_exe_start;
extern unsigned char WEAK _binary_loader_m32_exe_end;

/**
 * Extract the built-in loader.  This function returns NULL if an
 * error occurred, otherwise it returns the path to the extracted
 * loader.  Note: @tracee is only used for notification purpose.
 */
static char *extract_loader(const Tracee *tracee, bool wants_32bit_version)
{
	char path[PATH_MAX];
	size_t status2;
	void *start;
	size_t size;
	int status;
	int fd;

	char *loader_path = NULL;
	FILE *file = NULL;

	file = open_temp_file(NULL, "prooted");
	if (file == NULL)
		goto end;
	fd = fileno(file);

	if (wants_32bit_version) {
		start = (void *) &_binary_loader_m32_exe_start;
		size  = (size_t)(&_binary_loader_m32_exe_end-&_binary_loader_m32_exe_start);
	}
	else {
		start = (void *) &_binary_loader_exe_start;
		size  = (size_t) (&_binary_loader_exe_end-&_binary_loader_exe_start);
	}

	status2 = write(fd, start, size);
	if (status2 != size) {
		note(tracee, ERROR, SYSTEM, "can't write the loader");
		goto end;
	}

	status = fchmod(fd, S_IRUSR|S_IXUSR);
	if (status < 0) {
		note(tracee, ERROR, SYSTEM, "can't change loader permissions (u+rx)");
		goto end;
	}

	status = readlink_proc_pid_fd(getpid(), fd, path);
	if (status < 0) {
		note(tracee, ERROR, INTERNAL, "can't retrieve loader path (/proc/self/fd/)");
		goto end;
	}

	status = access(path, X_OK);
	if (status < 0) {
		note(tracee, ERROR, INTERNAL,
			"it seems the current temporary directory (%s) "
			"is mounted with no execution permission.",
			get_temp_directory());
		note(tracee, INFO, USER,
			"Please set PROOT_TMP_DIR env. variable to an alternate "
			"location ('%s/tmp' for example).", get_root(tracee));
		goto end;
	}

	loader_path = talloc_strdup(talloc_autofree_context(), path);
	if (loader_path == NULL) {
		note(tracee, ERROR, INTERNAL, "can't allocate memory");
		goto end;
	}

	if (tracee->verbose >= 2)
		note(tracee, INFO, INTERNAL, "loader: %s", loader_path);

end:
	if (file != NULL) {
		status = fclose(file);
		if (status < 0)
			note(tracee, WARNING, SYSTEM, "can't close loader file");
	}

	return loader_path;
}

/**
 * Get the path to the loader for the given @tracee.  This function
 * returns NULL if an error occurred.
 */
static inline const char *get_loader_path(const Tracee *tracee)
{
	static char *loader_path = NULL;

#if defined(HAS_LOADER_32BIT)
	static char *loader32_path = NULL;

	if (IS_CLASS32(tracee->load_info->elf_header)) {
		loader32_path = loader32_path ?: getenv("PROOT_LOADER_32") ?: extract_loader(tracee, true);
		return loader32_path;
	}
	else
#endif
	{
		loader_path = loader_path ?: getenv("PROOT_LOADER") ?: extract_loader(tracee, false);
		return loader_path;
	}
}

/**
 * Extract all the information that will be required by
 * translate_load_*().  This function returns -errno if an error
 * occured, otherwise 0.
 */
int translate_execve_enter(Tracee *tracee)
{
	char user_path[PATH_MAX];
	char host_path[PATH_MAX];
	char new_exe[PATH_MAX];
	char *raw_path;
	const char *loader_path;
	int status;

	if (IS_NOTIFICATION_PTRACED_LOAD_DONE(tracee)) {
		/* Syscalls can now be reported to its ptracer.  */
		tracee->as_ptracee.ignore_loader_syscalls = false;

		/* Cancel this spurious execve, it was only used as a
		 * notification.  */
		set_sysnum(tracee, PR_void);
		return 0;
	}

	status = get_sysarg_path(tracee, user_path, SYSARG_1);
	if (status < 0)
		return status;

	/* Remember the user path before it is overwritten by
	 * expand_shebang().  This "raw" path is useful to fix the
	 * value of AT_EXECFN and /proc/{@tracee->pid}/comm.  */
	raw_path = talloc_strdup(tracee->ctx, user_path);
	if (raw_path == NULL)
		return -ENOMEM;

	status = expand_shebang(tracee, host_path, user_path);
	if (status < 0)
		/* The Linux kernel actually returns -EACCES when
		 * trying to execute a directory.  */
		return status == -EISDIR ? -EACCES : status;

	/* user_path is modified only if there's an interpreter
	 * (ie. for a script or with qemu).  */
	if (status == 0 && tracee->qemu == NULL)
		TALLOC_FREE(raw_path);

	/* Remember the new value for "/proc/self/exe".  It points to
	 * a canonicalized guest path, hence detranslate_path()
	 * instead of using user_path directly.  */
	strcpy(new_exe, host_path);
	status = detranslate_path(tracee, new_exe, NULL);
	if (status >= 0) {
		talloc_unlink(tracee, tracee->new_exe);
		tracee->new_exe = talloc_strdup(tracee, new_exe);
	}
	else
		tracee->new_exe = NULL;

	if (tracee->qemu != NULL) {
		status = expand_runner(tracee, host_path, user_path);
		if (status < 0)
			return status;
	}

	TALLOC_FREE(tracee->load_info);

	tracee->load_info = talloc_zero(tracee, LoadInfo);
	if (tracee->load_info == NULL)
		return -ENOMEM;

	tracee->load_info->host_path = talloc_strdup(tracee->load_info, host_path);
	if (tracee->load_info->host_path == NULL)
		return -ENOMEM;

	tracee->load_info->user_path = talloc_strdup(tracee->load_info, user_path);
	if (tracee->load_info->user_path == NULL)
		return -ENOMEM;

	tracee->load_info->raw_path = (raw_path != NULL
			? talloc_reparent(tracee->ctx, tracee->load_info, raw_path)
			: talloc_reference(tracee->load_info, tracee->load_info->user_path));
	if (tracee->load_info->raw_path == NULL)
		return -ENOMEM;

	status = extract_load_info(tracee, tracee->load_info);
	if (status < 0)
		return status;

	if (tracee->load_info->interp != NULL) {
		status = extract_load_info(tracee, tracee->load_info->interp);
		if (status < 0)
			return status;

		/* An ELF interpreter is supposed to be
		 * standalone.  */
		if (tracee->load_info->interp->interp != NULL)
			return -EINVAL;
	}

	compute_load_addresses(tracee);

	/* Execute the loader instead of the program.  */
	loader_path = get_loader_path(tracee);
	if (loader_path == NULL)
		return -ENOENT;

	status = set_sysarg_path(tracee, loader_path, SYSARG_1);
	if (status < 0)
		return status;

	/* Mask to its ptracer syscalls performed by the loader.  */
	tracee->as_ptracee.ignore_loader_syscalls = true;

	return 0;
}
