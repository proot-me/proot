/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
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

#ifndef EXTRACT_H
#define EXTRACT_H

#include <stdint.h>
#include "attribute.h"

#define AUTOEXTRACT_SIGNATURE "I_LOVE_PIZZA"

typedef struct {
	char signature[sizeof(AUTOEXTRACT_SIGNATURE)];
	uint64_t size;
} PACKED AutoExtractInfo;

extern int WEAK extract_archive_from_file(const char *path);

#endif /* EXTRACT_H */
