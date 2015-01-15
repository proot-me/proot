/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of WioM.
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

#ifndef WIOM_FORMAT_H
#define WIOM_FORMAT_H

#include <talloc.h>

#include "extension/wiom/wiom.h"

extern void report_events_dump(FILE *file, const Event *history);
extern int replay_events_dump(TALLOC_CTX *context, SharedConfig *config);

extern void report_events_trace(FILE *file, const Event *history);
extern void report_events_fs_state(FILE *file, const Event *history);

#endif /* WIOM_FORMAT_H */
