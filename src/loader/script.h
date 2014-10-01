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

#ifndef SCRIPT
#define SCRIPT

#define LOAD_PACKET_ACTION(pointer)	(pointer)[0]
#define LOAD_PACKET_ADDR(pointer)	(pointer)[1]
#define LOAD_PACKET_ADDR2(pointer)	(pointer)[2]
#define LOAD_PACKET_LENGTH(pointer)	(pointer)[2]
#define LOAD_PACKET_PROT(pointer)	(pointer)[3]
#define LOAD_PACKET_OFFSET(pointer)	(pointer)[4]

#define LOAD_ACTION_OPEN		0
#define LOAD_ACTION_CLOSE_OPEN		1
#define LOAD_ACTION_CLOSE_BRANCH	2
#define LOAD_ACTION_MMAP_FILE		3
#define LOAD_ACTION_MMAP_ANON		4
#define LOAD_ACTION_CLEAR		5

#define LOAD_PACKET_LENGTH_OPEN		2 /* action, addr */
#define LOAD_PACKET_LENGTH_CLOSE_OPEN	2 /* action, addr */
#define LOAD_PACKET_LENGTH_CLOSE_BRANCH	3 /* action, addr, addr2 */
#define LOAD_PACKET_LENGTH_MMAP_FILE	5 /* action, addr, length, prot, offset */
#define LOAD_PACKET_LENGTH_MMAP_ANON	4 /* action, addr, length, prot, */
#define LOAD_PACKET_LENGTH_CLEAR	3 /* action, addr, length */

#endif /* SCRIPT */
