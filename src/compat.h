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

#ifndef COMPAT_H
#define COMPAT_H

/* Local definitions for compatibility with old and/or broken distros... */
#    ifndef AT_FDCWD
#        define AT_FDCWD		-100
#    endif
#    ifndef AT_SYMLINK_FOLLOW
#        define AT_SYMLINK_FOLLOW	0x400
#    endif
#    ifndef AT_REMOVEDIR
#        define AT_REMOVEDIR		0x200
#    endif
#    ifndef AT_SYMLINK_NOFOLLOW
#        define AT_SYMLINK_NOFOLLOW	0x100
#    endif
#    ifndef IN_DONT_FOLLOW
#        define IN_DONT_FOLLOW		0x02000000
#    endif
#    ifndef WIFCONTINUED
#        define WIFCONTINUED(status)	((status) == 0xffff)
#    endif
#    ifndef PTRACE_SETOPTIONS
#        define PTRACE_SETOPTIONS	0x4200
#    endif
#    ifndef PTRACE_GETEVENTMSG
#        define PTRACE_GETEVENTMSG	0x4201
#    endif
#    ifndef PTRACE_O_TRACESYSGOOD
#        define PTRACE_O_TRACESYSGOOD	0x00000001
#    endif
#    ifndef PTRACE_O_TRACEFORK
#        define PTRACE_O_TRACEFORK	0x00000002
#    endif
#    ifndef PTRACE_O_TRACEVFORK
#        define PTRACE_O_TRACEVFORK	0x00000004
#    endif
#    ifndef PTRACE_O_TRACECLONE
#        define PTRACE_O_TRACECLONE	0x00000008
#    endif
#    ifndef PTRACE_O_TRACEEXEC
#        define PTRACE_O_TRACEEXEC	0x00000010
#    endif
#    ifndef PTRACE_O_TRACEEXIT
#        define PTRACE_O_TRACEEXIT	0x00000040
#    endif
#    ifndef PTRACE_EVENT_FORK
#        define PTRACE_EVENT_FORK	1
#    endif
#    ifndef PTRACE_EVENT_VFORK
#        define PTRACE_EVENT_VFORK	2
#    endif
#    ifndef PTRACE_EVENT_CLONE
#        define PTRACE_EVENT_CLONE	3
#    endif
#    ifndef PTRACE_EVENT_EXEC
#        define PTRACE_EVENT_EXEC	4
#    endif
#    ifndef PTRACE_EVENT_EXIT
#        define PTRACE_EVENT_EXIT	6
#    endif
#    ifndef ADDR_NO_RANDOMIZE
#        define ADDR_NO_RANDOMIZE	0x0040000
#    endif
#    ifndef TALLOC_FREE
#        define TALLOC_FREE(ctx) do { talloc_free(ctx); ctx = NULL; } while(0)
#    endif
#    ifndef talloc_get_type_abort
#        define talloc_get_type_abort talloc_get_type
#    endif
#endif /* COMPAT_H */
