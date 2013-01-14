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

/* Note: syscalls like "openat" can be replaced by "open" since PRoot
 * has canonicalized "fd + path" into "path".  */
switch (peek_reg(tracee, CURRENT, SYSARG_NUM)) {
case PR_accept4: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,28),
		.new_sysarg_num   = PR_accept,
		.shifts		  = {{0}}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_dup3: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,27),
		.new_sysarg_num   = PR_dup2,
		.shifts		  = {{0}}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_epoll_create1: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,27),
		.new_sysarg_num   = PR_epoll_create,
		.shifts		  = {{0}}
	};
	poke_reg(tracee, SYSARG_1, 0); /* Force "size" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_epoll_pwait: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,19),
		.new_sysarg_num   = PR_epoll_wait,
		.shifts		  = {{0}}
	};
	poke_reg(tracee, SYSARG_5, 0); /* Force "sigmask" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_eventfd2: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,27),
		.new_sysarg_num   = PR_eventfd,
		.shifts		  = {{0}}
	};
	poke_reg(tracee, SYSARG_2, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_faccessat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_access,
		.shifts	= { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 2,
				.offset  = -1 }
		}
	};
	poke_reg(tracee, SYSARG_4, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_fchmodat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_chmod,
		.shifts	= { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 2,
				.offset  = -1 }
		}
	};
	poke_reg(tracee, SYSARG_4, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_fchownat: {
	word_t flags;
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.shifts	= { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 3,
				.offset  = -1 }
		}
	};

	flags = peek_reg(tracee, CURRENT, SYSARG_5);
	modif.new_sysarg_num = ((flags & AT_SYMLINK_NOFOLLOW) != 0
				? PR_lchown
				: PR_chown);

	poke_reg(tracee, SYSARG_5, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_newfstatat:
case PR_fstatat64: {
	word_t flags;
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 2,
				.offset  = -1 }
		}
	};

	flags = peek_reg(tracee, CURRENT, SYSARG_4);
	modif.new_sysarg_num = ((flags & AT_SYMLINK_NOFOLLOW) != 0
				? PR_lstat
#if defined(ARCH_X86_64)
				: PR_stat);
#else
				: PR_stat64);
#endif

	poke_reg(tracee, SYSARG_4, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_futimesat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_utimes,
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 2,
				.offset  = -1 }
		}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_inotify_init1: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,27),
		.new_sysarg_num   = PR_inotify_init,
		.shifts		  = {{0}}
	};
	poke_reg(tracee, SYSARG_1, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_linkat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_link,
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 1,
				.offset  = -1 },
			    [1] = {
				.sysarg  = SYSARG_4,
				.nb_args = 1,
				.offset  = -2 }
		}
	};
	poke_reg(tracee, SYSARG_5, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_mkdirat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_mkdir,
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 2,
				.offset  = -1 }
		}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_mknodat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_mknod,
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 3,
				.offset  = -1 }
		}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_openat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_open,
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 3,
				.offset  = -1 }
		}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_pipe2: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,27),
		.new_sysarg_num   = PR_pipe,
		.shifts		  = {{0}}
	};
	poke_reg(tracee, SYSARG_2, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_pselect6: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,19),
#if defined(ARCH_X86_64)
		.new_sysarg_num   = PR_select,
#else
		.new_sysarg_num   = PR__newselect,
#endif
		.shifts		  = {{0}}
	};
	poke_reg(tracee, SYSARG_6, 0); /* Force "sigmask" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_readlinkat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_readlink,
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 3,
				.offset  = -1}
		}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_renameat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_rename,
		.shifts = { [0] = {
				.sysarg  = SYSARG_2,
				.nb_args = 1,
				.offset  =-1 },
			    [1] = {
				.sysarg  = SYSARG_4,
				.nb_args = 1,
				.offset  = -2 }
		}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_signalfd4: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,17),
		.new_sysarg_num   = PR_signalfd,
		.shifts		  = {{0}}
	};
	poke_reg(tracee, SYSARG_3, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_symlinkat: {
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.new_sysarg_num   = PR_symlink,
		.shifts = { [0] = {
				.sysarg  = SYSARG_3,
				.nb_args = 1,
				.offset  = -1 }
		}
	};
	modify_syscall(tracee, config, &modif);
	return 0;
}

case PR_unlinkat: {
	word_t flags;
	Modif modif = {
		.expected_release = KERNEL_VERSION(2,6,16),
		.shifts = { [0] = {
				.sysarg  =  SYSARG_2,
				.nb_args = 1,
				.offset  = -1
			}
		}
	};

	flags = peek_reg(tracee, CURRENT, SYSARG_3);
	modif.new_sysarg_num = ((flags & AT_REMOVEDIR) != 0
				? PR_rmdir
				: PR_unlink);

	poke_reg(tracee, SYSARG_3, 0); /* Force "flags" to 0.  */
	modify_syscall(tracee, config, &modif);
	return 0;
}

default:
	return 0;
}
