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

#include "build.h"

#if defined(HAVE_SECCOMP_FILTER)

#include <sys/prctl.h>     /* prctl(2), PR_* */
#include <linux/seccomp.h> /* SECCOMP_MODE_FILTER, */
#include <linux/filter.h>  /* struct sock_*, */

#include "seccomp/seccomp.h"
#include "seccomp/filter.h"
#include "tracee/tracee.h"
#include "notice.h"
#include "arch.h"

/**
 * Create and install the following filter (pseudo-code) for the given
 * @tracee and all of its future children:
 *
 *     for each handled architectures
 *         for each handled syscall
 *             trace
 *         allow
 *     kill
 */
void configure_seccomp_filter(const Tracee *tracee)
{
	int status;
	struct sock_fprog program = { .len = 0, .filter = NULL };

	status = new_filter(&program);
	if (status < 0)
		goto end;

	#include SYSNUM_HEADER
	#include "seccomp/section.c"
#ifdef SYSNUM_HEADER2
	#include SYSNUM_HEADER2
	#include "seccomp/section.c"
#endif
#ifdef SYSNUM_HEADER3
	#include SYSNUM_HEADER3
	#include "seccomp/section.c"
#endif
	#include "syscall/sysnum-undefined.h"

	status = finalize_filter(&program);
	if (status < 0)
		goto end;

	status = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	if (status < 0) {
		notice(tracee, WARNING, SYSTEM, "prctl(PR_SET_NO_NEW_PRIVS)");
		goto end;
	}

	/* To output this BPF program for debug purpose:
	 *
	 *     write(2, program.filter, program.len * sizeof(struct sock_filter));
	 */

	status = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program);
	if (status < 0) {
		notice(tracee, WARNING, SYSTEM, "prctl(PR_SET_SECCOMP)");
		goto end;
	}

end:
	free_filter(&program);
	return;
}

#endif /* defined(HAVE_SECCOMP_FILTER) */
