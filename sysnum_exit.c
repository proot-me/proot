/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot: a PTrace based chroot alike.
 *
 * Copyright (C) 2010, 2011 STMicroelectronics
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
 *
 * Author: Cedric VINCENT (cedric.vincent@st.com)
 */

switch (child->sysnum) {
case __NR_getcwd:
	result = peek_ureg(child, SYSARG_RESULT);
	if (errno != 0) {
		status = -errno;
		goto end;
	}
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	status = detranslate_addr(child, child->output, result, GETCWD);
	if (status < 0)
		break;

	status = poke_ureg(child, SYSARG_RESULT, (word_t) status);
	if (status < 0)
		goto end;

	break;

case __NR_readlink:
case __NR_readlinkat:
	result = peek_ureg(child, SYSARG_RESULT);
	if (errno != 0) {
		status = -errno;
		goto end;
	}
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	/* Avoid the detranslation of partial result. */
	status = (int) peek_ureg(child, child->sysnum == __NR_readlink ? SYSARG_3 : SYSARG_4);
	if (errno != 0) {
		status = -errno;
		goto end;
	}
	if ((int) result == status) {
		status = 0;
		goto end;
	}

	assert((int) result <= status);

	status = detranslate_addr(child, child->output, result, READLINK);
	if (status < 0)
		break;

	status = poke_ureg(child, SYSARG_RESULT, (word_t) status);
	if (status < 0)
		goto end;

	break;

default:
	status = 0;
	goto end;
}
