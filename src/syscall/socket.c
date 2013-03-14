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

#include <stddef.h>      /* offsetof(3), */
#include <strings.h>     /* bzero(3), */
#include <string.h>      /* strncpy(3), strlen(3), */
#include <assert.h>      /* assert(3), */
#include <errno.h>       /* E*, */
#include <sys/socket.h>  /* struct sockaddr_un, AF_UNIX, */
#include <sys/un.h>      /* struct sockaddr_un, */

#include "syscall/socket.h"
#include "tracee/tracee.h"
#include "tracee/mem.h"
#include "path/path.h"
#include "arch.h"

/* The sockaddr_un structure has exactly the same layout on all
 * architectures.  */
static const off_t offsetof_path = offsetof(struct sockaddr_un, sun_path);
static struct sockaddr_un sockaddr_un__;
static const size_t sizeof_path  = sizeof(sockaddr_un__.sun_path);

/**
 * Copy in @sockaddr the struct sockaddr_un stored in the @tracee
 * memory at the given @address.  Also, its pathname is copied to the
 * null-terminated @path.  Only @size bytes are read from the @tracee
 * memory (should be <= @max_size <= sizeof(struct sockaddr_un)).
 * This function returns -errno if an error occurred, 0 if the
 * structure was not found (not a sockaddr_un or @size > @max_size),
 * otherwise 1.
 */
static int read_sockaddr_un(Tracee *tracee, struct sockaddr_un *sockaddr, word_t max_size,
			char path[PATH_MAX], word_t address, int size)
{
	int status;

	assert(max_size <= sizeof(struct sockaddr_un));

	/* Nothing to do if the sockaddr has an unexpected size.  */
	if (size <= offsetof_path || (word_t) size > max_size)
		return 0;

	bzero(sockaddr, sizeof(struct sockaddr_un));
	status = read_data(tracee, sockaddr, address, size);
	if (status < 0)
		return status;

	/* Nothing to do if it's not a named Unix domain socket.  */
	if ((sockaddr->sun_family != AF_UNIX)
	    || sockaddr->sun_path[0] == '\0')
		return 0;

	/* Be careful: sun_path doesn't have to be null-terminated.  */
	assert(sizeof_path < PATH_MAX - 1);
	strncpy(path, sockaddr->sun_path, sizeof_path);
	path[sizeof_path] = '\0';

	return 1;
}

/**
 * Translate the pathname of the struct sockaddr_un currently stored
 * in the @tracee memory at the given @address.  See the documentation
 * of read_sockaddr_un() for the meaning of the @size parameter.
 * Also, the new address of the translated sockaddr_un is put in the
 * @address parameter.  This function returns -errno if an error
 * occurred, otherwise 0.
 */
int translate_socketcall_enter(Tracee *tracee, word_t *address, int size)
{
	struct sockaddr_un sockaddr;
	char path2[PATH_MAX];
	char path[PATH_MAX];
	int status;

	if (*address == 0)
		return 0;

	status = read_sockaddr_un(tracee, &sockaddr, sizeof(sockaddr), path2, *address, size);
	if (status <= 0)
		return status;

	status = translate_path(tracee, path, AT_FDCWD, path2, true);
	if (status < 0)
		return status;

	/* Be careful: sun_path doesn't have to be null-terminated.  */
	if (strlen(path) > sizeof_path)
		return -EINVAL;
	strncpy(sockaddr.sun_path, path, sizeof_path);

	/* Push the updated sockaddr to a newly allocated space.  */
	*address = alloc_mem(tracee, sizeof(sockaddr));
	if (*address == 0)
		return -EFAULT;

	status = write_data(tracee, *address, &sockaddr, sizeof(sockaddr));
	if (status < 0)
		return status;

	return 1;
}

#if !defined(MIN)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/**
 * Detranslate the pathname of the struct sockaddr_un currently stored
 * in the @tracee memory at the given @sock_addr.  See the
 * documentation of read_sockaddr_un() for the meaning of the
 * @size_addr and @max_size parameters.  This function returns -errno
 * if an error occurred, otherwise 0.
 */
int translate_socketcall_exit(Tracee *tracee, word_t sock_addr, word_t size_addr, word_t max_size)
{
	struct sockaddr_un sockaddr;
	bool is_truncated = false;
	char path[PATH_MAX];
	int status;
	int size;

	if (sock_addr == 0)
		return 0;

	size = (int) peek_mem(tracee, size_addr);
	if (errno != 0)
		return -errno;

	max_size = MIN(max_size, sizeof(sockaddr));
	status = read_sockaddr_un(tracee, &sockaddr, max_size, path, sock_addr, size);
	if (status <= 0)
		return status;

	status = detranslate_path(tracee, path, NULL);
	if (status < 0)
		return status;

	/* Be careful: sun_path doesn't have to be null-terminated.  */
	size = offsetof_path + strlen(path) + 1;
	if (size < 0 || (word_t) size > max_size) {
		size = max_size;
		is_truncated = true;
	}
	strncpy(sockaddr.sun_path, path, sizeof_path);

	/* Overwrite the sockaddr and socklen parameters.  */
	status = write_data(tracee, sock_addr, &sockaddr, size);
	if (status < 0)
		return status;

	/* If sockaddr is truncated (because the buffer provided is
	 * too small), addrlen will return a value greater than was
	 * supplied to the call.  See man 2 accept. */
	if (is_truncated)
		size = max_size + 1;

	status = write_data(tracee, size_addr, &size, sizeof(size));
	if (status < 0)
		return status;

	return 0;
}
