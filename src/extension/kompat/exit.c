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

switch (peek_reg(tracee, ORIGINAL, SYSARG_NUM)) {
case PR_uname: {
	struct utsname utsname;
	word_t address;
	word_t result;
	size_t size;

	assert(config->release != NULL);

	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

	/* Error reported by the kernel.  */
	if ((int) result < 0)
		return 0;

	address = peek_reg(tracee, ORIGINAL, SYSARG_1);

	status = read_data(tracee, &utsname, address, sizeof(utsname));
	if (status < 0)
		return status;

	/* Note: on x86_64, we can handle the two modes (32/64) with
	 * the same code since struct utsname has always the same
	 * layout.  */
	size = sizeof(utsname.release);
	strncpy(utsname.release, config->release, size);
	utsname.release[size - 1] = '\0';

	status = write_data(tracee, address, &utsname, sizeof(utsname));
	if (status < 0)
		return status;

	return 0;
}

default:
	return 0;
}
