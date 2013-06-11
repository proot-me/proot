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

syscall_number = peek_reg(tracee, ORIGINAL, SYSARG_NUM);
switch (syscall_number) {
default:
	/* Nothing to do. */
	status = 0;
	break;

case PR_execve:
	status = translate_execve(tracee);
	break;

case PR_fchdir:
case PR_chdir: {
	char *tmp;

	if (syscall_number == PR_chdir) {
		status = get_sysarg_path(tracee, path, SYSARG_1);
		if (status < 0)
			break;

		/* The ending "." ensures canonicalize() will report
		 * an error if path does not exist or if it is not a
		 * directory.  */
		if (path[0] != '/')
			status = join_paths(3, newpath, tracee->fs->cwd, path, ".");
		else
			status = join_paths(2, newpath, path, ".");
		if (status < 0)
			break;

		/* Initiale state for canonicalization.  */
		strcpy(path, "/");

		status = canonicalize(tracee, newpath, true, path, 0);
		if (status < 0)
			break;
	}
	else {
		dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

		/* Sadly this method doesn't detranslate statefully,
		 * this means that there's an ambiguity when several
		 * bindings are from the same host path:
		 *
		 *    $ proot -m /tmp:/a -m /tmp:/b fchdir_getcwd /a
		 *    /b
		 *
		 *    $ proot -m /tmp:/b -m /tmp:/a fchdir_getcwd /a
		 *    /a
		 *
		 * A solution would be to follow each file descriptor
		 * just like it is done for cwd.
		 */

		status = translate_path(tracee, path, dirfd, ".", true);
		if (status < 0)
			break;

		status = detranslate_path(tracee, path, NULL);
		if (status < 0)
			break;
	}

	/* Remove the trailing "/" or "/.".  */
	chop_finality(path);

	tmp = talloc_strdup(tracee->fs, path);
	if (tmp == NULL) {
		status = -ENOMEM;
		break;
	}
	TALLOC_FREE(tracee->fs->cwd);

	tracee->fs->cwd = tmp;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");

	poke_reg(tracee, SYSARG_NUM, SYSCALL_AVOIDER);
	status = 0;
	break;
}

case PR_bind:
case PR_connect: {
	word_t address;
	word_t size;

	address = peek_reg(tracee, CURRENT, SYSARG_2);
	size    = peek_reg(tracee, CURRENT, SYSARG_3);

	status = translate_socketcall_enter(tracee, &address, size);
	if (status <= 0)
		break;

	poke_reg(tracee, SYSARG_2, address);
	poke_reg(tracee, SYSARG_3, sizeof(struct sockaddr_un));

	status = 0;
	break;
}

case PR_accept:
case PR_accept4:
case PR_getsockname:
case PR_getpeername:{
	int size;

	/* Remember: PEEK_MEM puts -errno in status and breaks if an
	 * error occured.  */
	size = (int) PEEK_MEM(peek_reg(tracee, ORIGINAL, SYSARG_3));

	/* The "size" argument is both used as an input parameter
	 * (max. size) and as an output parameter (actual size).  The
	 * exit stage needs to know the max. size to not overwrite
	 * anything, that's why it is copied in the 6th argument
	 * (unused) before the kernel updates it.  */
	poke_reg(tracee, SYSARG_6, size);

	status = 0;
	break;
}

case PR_socketcall: {
	word_t args_addr;
	word_t sock_addr_saved;
	word_t sock_addr;
	word_t size_addr;
	word_t size;

	args_addr = peek_reg(tracee, CURRENT, SYSARG_2);

	switch (peek_reg(tracee, CURRENT, SYSARG_1)) {
	case SYS_BIND:
	case SYS_CONNECT:
		/* Handle these cases below.  */
		status = 1;
		break;

	case SYS_ACCEPT:
	case SYS_ACCEPT4:
	case SYS_GETSOCKNAME:
	case SYS_GETPEERNAME:
		/* Remember: PEEK_MEM puts -errno in status and breaks
		 * if an error occured.  */
		size_addr =  PEEK_MEM(SYSARG_ADDR(3));
		size = (int) PEEK_MEM(size_addr);

		/* See case PR_accept for explanation.  */
		poke_reg(tracee, SYSARG_6, size);
		status = 0;
		break;

	default:
		status = 0;
		break;
	}

	/* An error occured or there's nothing else to do.  */
	if (status <= 0)
		break;

	/* Remember: PEEK_MEM puts -errno in status and breaks if an
	 * error occured.  */
	sock_addr = PEEK_MEM(SYSARG_ADDR(2));
	size      = PEEK_MEM(SYSARG_ADDR(3));

	sock_addr_saved = sock_addr;
	status = translate_socketcall_enter(tracee, &sock_addr, size);
	if (status <= 0)
		break;

	/* These parameters are used/restored at the exit stage.  */
	poke_reg(tracee, SYSARG_5, sock_addr_saved);
	poke_reg(tracee, SYSARG_6, size);

	/* Remember: POKE_MEM puts -errno in status and breaks if an
	 * error occured.  */
	POKE_MEM(SYSARG_ADDR(2), sock_addr);
	POKE_MEM(SYSARG_ADDR(3), sizeof(struct sockaddr_un));

	status = 0;
	break;
}

case PR_access:
case PR_acct:
case PR_chmod:
case PR_chown:
case PR_chown32:
case PR_chroot:
case PR_getxattr:
case PR_listxattr:
case PR_mknod:
case PR_oldstat:
case PR_creat:
case PR_removexattr:
case PR_setxattr:
case PR_stat:
case PR_stat64:
case PR_statfs:
case PR_statfs64:
case PR_swapoff:
case PR_swapon:
case PR_truncate:
case PR_truncate64:
case PR_umount:
case PR_umount2:
case PR_uselib:
case PR_utime:
case PR_utimes:
	status = translate_sysarg(tracee, SYSARG_1, REGULAR);
	break;

case PR_open:
	flags = peek_reg(tracee, CURRENT, SYSARG_2);

	if (   ((flags & O_NOFOLLOW) != 0)
	       || ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
		status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
	else
		status = translate_sysarg(tracee, SYSARG_1, REGULAR);
	break;

case PR_faccessat:
case PR_fchmodat:
case PR_fchownat:
case PR_fstatat64:
case PR_newfstatat:
case PR_utimensat:
case PR_name_to_handle_at:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	flags = (   syscall_number == PR_fchownat
		 || syscall_number == PR_name_to_handle_at)
		? peek_reg(tracee, CURRENT, SYSARG_5)
		: peek_reg(tracee, CURRENT, SYSARG_4);

	if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
		status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
	else
		status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
	break;

case PR_futimesat:
case PR_mknodat:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
	break;

case PR_inotify_add_watch:
	flags = peek_reg(tracee, CURRENT, SYSARG_3);

	if ((flags & IN_DONT_FOLLOW) != 0)
		status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
	else
		status = translate_sysarg(tracee, SYSARG_2, REGULAR);
	break;

case PR_readlink:
case PR_lchown:
case PR_lchown32:
case PR_lgetxattr:
case PR_llistxattr:
case PR_lremovexattr:
case PR_lsetxattr:
case PR_lstat:
case PR_lstat64:
case PR_oldlstat:
case PR_unlink:
case PR_rmdir:
case PR_mkdir:
	status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
	break;

case PR_pivot_root:
	status = translate_sysarg(tracee, SYSARG_1, REGULAR);
	if (status < 0)
		break;

	status = translate_sysarg(tracee, SYSARG_2, REGULAR);
	break;

case PR_linkat:
	olddirfd = peek_reg(tracee, CURRENT, SYSARG_1);
	newdirfd = peek_reg(tracee, CURRENT, SYSARG_3);
	flags    = peek_reg(tracee, CURRENT, SYSARG_5);

	status = get_sysarg_path(tracee, oldpath, SYSARG_2);
	if (status < 0)
		break;

	status = get_sysarg_path(tracee, newpath, SYSARG_4);
	if (status < 0)
		break;

	if ((flags & AT_SYMLINK_FOLLOW) != 0)
		status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, REGULAR);
	else
		status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, SYMLINK);
	if (status < 0)
		break;

	status = translate_path2(tracee, newdirfd, newpath, SYSARG_4, SYMLINK);
	break;

case PR_mount:
	status = get_sysarg_path(tracee, path, SYSARG_1);
	if (status < 0)
		break;

	/* The following check covers only 90% of the cases. */
	if (path[0] == '/' || path[0] == '.') {
		status = translate_path2(tracee, AT_FDCWD, path, SYSARG_1, REGULAR);
		if (status < 0)
			break;
	}

	status = translate_sysarg(tracee, SYSARG_2, REGULAR);
	break;

case PR_openat:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);
	flags = peek_reg(tracee, CURRENT, SYSARG_3);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	if (   ((flags & O_NOFOLLOW) != 0)
	       || ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
		status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
	else
		status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
	break;

case PR_readlinkat:
case PR_unlinkat:
case PR_mkdirat:
	dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

	status = get_sysarg_path(tracee, path, SYSARG_2);
	if (status < 0)
		break;

	status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
	break;

case PR_link:
case PR_rename:
	status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
	if (status < 0)
		break;

	status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
	break;

case PR_renameat:
	olddirfd = peek_reg(tracee, CURRENT, SYSARG_1);
	newdirfd = peek_reg(tracee, CURRENT, SYSARG_3);

	status = get_sysarg_path(tracee, oldpath, SYSARG_2);
	if (status < 0)
		break;

	status = get_sysarg_path(tracee, newpath, SYSARG_4);
	if (status < 0)
		break;

	status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, SYMLINK);
	if (status < 0)
		break;

	status = translate_path2(tracee, newdirfd, newpath, SYSARG_4, SYMLINK);
	break;

case PR_symlink:
	status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
	break;

case PR_symlinkat:
	newdirfd = peek_reg(tracee, CURRENT, SYSARG_2);

	status = get_sysarg_path(tracee, newpath, SYSARG_3);
	if (status < 0)
		break;

	status = translate_path2(tracee, newdirfd, newpath, SYSARG_3, SYMLINK);
	break;
}
