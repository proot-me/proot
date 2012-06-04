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

#include <sys/ptrace.h> /* ptrace(2), PTRACE_*, */
#include <sys/types.h>  /* pid_t, size_t, */
#include <stdlib.h>     /* NULL, exit(3), */
#include <stddef.h>     /* offsetof(), */
#include <sys/user.h>   /* struct user*, */
#include <errno.h>      /* errno, */
#include <limits.h>     /* ULONG_MAX, */
#include <assert.h>     /* assert(3), */
#include <sys/wait.h>   /* waitpid(2), */

#include "tracee/mem.h"
#include "arch.h"    /* REG_SYSARG_*, word_t, NO_MISALIGNED_ACCESS, SIZEOF_WORD */
#include "syscall/syscall.h" /* USER_REGS_OFFSET, */
#include "tracee/ureg.h"    /* peek_ureg(), poke_ureg(), */
#include "notice.h"

static inline word_t read_mem_word(unsigned char *ptr) {
	return *(word_t *)ptr;
}

static inline void write_mem_word(unsigned char *ptr, word_t src) {
	*((word_t *)ptr) = src;
}

#ifdef NO_MISALIGNED_ACCESS
#if SIZEOF_WORD < 4 || SIZEOF_WORD > 8 || SIZEOF_WORD % 4 != 0
#error "unimplemented functionality for this SIZEOF_WORD"
#endif
static inline word_t get_word(unsigned char *ptr) {
	union {
		unsigned char c[sizeof(word_t)];
		word_t w;
	} u;
	if (((word_t)ptr) % sizeof(word_t) == 0)
		return read_mem_word(ptr);

	u.c[0] = ptr[0]; u.c[1] = ptr[1];
	u.c[2] = ptr[2]; u.c[3] = ptr[3];
#if SIZEOF_WORD == 8
	u.c[4] = ptr[4]; u.c[5] = ptr[5];
	u.c[6] = ptr[6]; u.c[7] = ptr[7];
#endif
	return u.w;
}
static inline void set_word(unsigned char *ptr, word_t src) {
	union {
		unsigned char c[sizeof(word_t)];
		word_t w;
	} u;
	if (((word_t)ptr) % sizeof(word_t) == 0) {
		write_mem_word(ptr, src);
		return;
	}

	u.w = src;
	ptr[0] = u.c[0]; ptr[1] = u.c[1];
	ptr[2] = u.c[2]; ptr[3] = u.c[3];
#if SIZEOF_WORD == 8
	ptr[4] = u.c[4]; ptr[5] = u.c[5];
	ptr[6] = u.c[6]; ptr[7] = u.c[7];
#endif
	return;
}
#else
static inline word_t get_word(unsigned char *ptr) {
	return read_mem_word(ptr);
}
static inline void set_word(unsigned char *ptr, word_t src) {
	write_mem_word(ptr, src);
}
#endif

/**
 * Resize by @size bytes the stack of the @tracee. This function
 * returns 0 if an error occured, otherwise it returns the address of
 * the new stack pointer within the tracee's memory space.
 */
word_t resize_tracee_stack(struct tracee_info *tracee, ssize_t size)
{
	word_t stack_pointer;
	long status;

	/* Get the current value of the stack pointer from the tracee's
	 * USER area. */
	stack_pointer = peek_ureg(tracee, STACK_POINTER);
	if (errno != 0)
		return 0;

	/* Sanity check. */
	if (   (size > 0 && stack_pointer <= size)
	    || (size < 0 && stack_pointer >= ULONG_MAX + size)) {
		notice(WARNING, INTERNAL, "integer under/overflow detected in %s", __FUNCTION__);
		return 0;
	}

	/* Remember the stack grows downward. */
	stack_pointer -= size;

	/* Set the new value of the stack pointer in the tracee's USER
	 * area. */
	status = poke_ureg(tracee, STACK_POINTER, stack_pointer);
	if (status < 0)
		return 0;

	return stack_pointer;
}

/**
 * Copy @size bytes from the buffer @src_tracer to the address
 * @dest_tracee within the memory space of the @tracee process. It
 * returns -errno if an error occured, otherwise 0.
 */
int copy_to_tracee(struct tracee_info *tracee, word_t dest_tracee, const void *src_tracer, word_t size)
{
	unsigned char *src  = (unsigned char *)src_tracer;
	unsigned char *dest = (unsigned char *)dest_tracee;

	long   status;
	word_t word, i, j;
	word_t nb_trailing_bytes;
	word_t nb_full_words;

	unsigned char *last_dest_word;

	nb_trailing_bytes = size % sizeof(word_t);
	nb_full_words     = (size - nb_trailing_bytes) / sizeof(word_t);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		status = ptrace(PTRACE_POKEDATA, tracee->pid, dest + i*sizeof(word_t),
				get_word(&src[i*sizeof(word_t)]));
		if (status < 0) {
			notice(WARNING, SYSTEM, "ptrace(POKEDATA)");
			return -EFAULT;
		}
	}

	/* Copy the bytes in the last word carefully since we have
	 * overwrite only the relevant ones. */

	word = ptrace(PTRACE_PEEKDATA, tracee->pid, dest + i*sizeof(word_t), NULL);
	if (errno != 0) {
		notice(WARNING, SYSTEM, "ptrace(PEEKDATA)");
		return -EFAULT;
	}

	last_dest_word = (unsigned char *)&word;
	for (j = 0; j < nb_trailing_bytes; j++)
		last_dest_word[j] = src[i*sizeof(word_t) + j];

	status = ptrace(PTRACE_POKEDATA, tracee->pid, dest + i*sizeof(word_t), word);
	if (status < 0) {
		notice(WARNING, SYSTEM, "ptrace(POKEDATA)");
		return -EFAULT;
	}

	return 0;
}

/**
 * Copy @size bytes to the buffer @dest_tracer from the address
 * @src_tracee within the memory space of the @tracee process. It
 * returns -errno if an error occured, otherwise 0.
 */
int copy_from_tracee(struct tracee_info *tracee, void *dest_tracer, word_t src_tracee, word_t size)
{
	unsigned char *src  = (unsigned char *)src_tracee;
	unsigned char *dest = (unsigned char *)dest_tracer;

	word_t nb_trailing_bytes;
	word_t nb_full_words;
	word_t word, i, j;

	unsigned char *last_src_word;

	nb_trailing_bytes = size % sizeof(word_t);
	nb_full_words     = (size - nb_trailing_bytes) / sizeof(word_t);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		word = ptrace(PTRACE_PEEKDATA, tracee->pid, src + i*sizeof(word_t), NULL);
		if (errno != 0) {
			notice(WARNING, SYSTEM, "ptrace(PEEKDATA)");
			return -EFAULT;
		}
		set_word(&dest[i*sizeof(word_t)], word);
	}

	/* Copy the bytes from the last word carefully since we have
	 * to not overwrite the bytes lying beyond the @to buffer. */

	word = ptrace(PTRACE_PEEKDATA, tracee->pid, src + i*sizeof(word_t), NULL);
	if (errno != 0) {
		notice(WARNING, SYSTEM, "ptrace(PEEKDATA)");
		return -EFAULT;
	}

	last_src_word  = (unsigned char *)&word;

	for (j = 0; j < nb_trailing_bytes; j++)
		dest[i*sizeof(word_t) + j] = last_src_word[j];

	return 0;
}

/**
 * Copy to @dest_tracer at most @max_size bytes from the string
 * pointed to by @src_tracee within the memory space of the @tracee
 * process. This function returns -errno on error, otherwise
 * it returns the number in bytes of the string, including the
 * end-of-string terminator.
 */
int get_tracee_string(struct tracee_info *tracee, void *dest_tracer, word_t src_tracee, word_t max_size)
{
	unsigned char *src  = (unsigned char *)src_tracee;
	unsigned char *dest = (unsigned char *)dest_tracer;

	word_t nb_trailing_bytes;
	word_t nb_full_words;
	word_t word, i, j;

	unsigned char *src_word;

	nb_trailing_bytes = max_size % sizeof(word_t);
	nb_full_words     = (max_size - nb_trailing_bytes) / sizeof(word_t);

	/* Copy one word by one word, except for the last one. */
	for (i = 0; i < nb_full_words; i++) {
		word = ptrace(PTRACE_PEEKDATA, tracee->pid, src + i*sizeof(word_t), NULL);
		if (errno != 0)
			return -EFAULT;

		set_word(&dest[i*sizeof(word_t)], word);

		/* Stop once an end-of-string is detected. */
		src_word = (unsigned char *)&word;
		for (j = 0; j < sizeof(word_t); j++)
			if (src_word[j] == '\0')
				return i * sizeof(word_t) + j + 1;
	}

	/* Copy the bytes from the last word carefully since we have
	 * to not overwrite the bytes lying beyond the @to buffer. */

	word = ptrace(PTRACE_PEEKDATA, tracee->pid, src + i*sizeof(word_t), NULL);
	if (errno != 0)
		return -EFAULT;

	src_word  = (unsigned char *)&word;
	for (j = 0; j < nb_trailing_bytes; j++) {
		dest[i*sizeof(word_t) + j] = src_word[j];
		if (src_word[j] == '\0')
			break;
	}

	return i * sizeof(word_t) + j + 1;
}
