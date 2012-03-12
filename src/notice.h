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

#ifndef NOTICE_H
#define NOTICE_H

/* Specify where a notice is coming from. */
enum notice_origin
{
	SYSTEM,
	INTERNAL,
	USER,
};

/* Specify the severity of a notice. */
enum notice_severity
{
	ERROR,
	WARNING,
	INFO,
};

#define VERBOSE(level, message, args...) do { if (config.verbose_level >= (level)) notice(INFO, INTERNAL, (message), ## args); } while (0)

extern void notice(enum notice_severity severity, enum notice_origin origin, const char *message, ...);

#endif /* NOTICE_H */
