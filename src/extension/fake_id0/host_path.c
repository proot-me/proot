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

Config *config = talloc_get_type_abort(extension->config, Config);
const char *path = (char*)data1;


/* If the syscall is flaged as 'interesting', record the current file
 * permissions */
if (config->interesting_syscall) {

	/* Create the list of nodes if needed */
	if (config->modified_nodes == NULL) {
		config->modified_nodes = talloc_zero(config, ModifiedNodes);
		if (config->modified_nodes == NULL)
			return 0;
		talloc_set_destructor(config->modified_nodes, modified_nodes_free);
	}

	/* Get the meta-data */
	struct stat perms;
	if (stat(path, &perms))
		return 0;

	/* Copy the current permissions */
	mode_t new_mode = perms.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);

	/* Add read and wite permissions to everything */
	new_mode |= (S_IRUSR | S_IWUSR);

	/* Always add 'x' bit to directories */
	if (S_ISDIR(perms.st_mode))
		new_mode |= S_IXUSR;


	/* Patch the permissions if needed */
	if (new_mode != (perms.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) {
		chmod(path, new_mode);

		/* Add the path to the list of modified nodes */
		ModifiedNode *node = talloc_zero(NULL, ModifiedNode);
		if (node == NULL)
			return 0;

		node->path = talloc_strdup(node, path);
		node->old_mode = perms.st_mode;
		SLIST_INSERT_HEAD(config->modified_nodes, node, link);
	}
}
