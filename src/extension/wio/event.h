/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2014 STMicroelectronics
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

#ifndef WIO_EVENT_H
#define WIO_EVENT_H

typedef enum {
	TRAVERSES,
	CREATES,
	DELETES,
	GETS_METADATA_OF,
	SETS_METADATA_OF,
	GETS_CONTENT_OF,
	SETS_CONTENT_OF,
	EXECUTES,
	MOVES,
	IS_CLONED,
} Event;

extern void record_event(pid_t pid, Event event, ...);

#endif /* WIO_EVENT_H */
