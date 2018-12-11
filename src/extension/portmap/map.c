/*
 * Copyright (C) 2016 Vincent Hage
 */

#include <arpa/inet.h>      /* inet_ntop */

#include "cli/note.h"
#include "extension/portmap/portmap.h"

/**
 * Set all entries empty by setting their key and values to PORTMAP_DEFAULT_VALUE.
 * The table mask is used in get_index as a fast way of doing the modulus operation.
 */
void initialize_portmap(PortMap *portmap)
{
	int i;

	for(i = 0; i < PORTMAP_SIZE; i++) {
		portmap->map[i].port_in = PORTMAP_DEFAULT_VALUE;
		portmap->map[i].port_out = PORTMAP_DEFAULT_VALUE;
	}

	portmap->table_mask = PORTMAP_SIZE - 1;
}

/**
 * Find an entry that is either empty or has the same key.
 * Return the index is successful, or PORTMAP_DEFAULT_VALUE otherwise
 */
uint16_t get_index(PortMap *portmap, uint16_t key)
{
	int i = 0;
	uint16_t index;

	/* the table mask is used instead of the mod operation
	 * to removed the unecessary bits of a number, to get an index. */
	index = key & portmap->table_mask;

	/* we go through the map until either:
	 *  1. the end of the map is reached
	 *  2. an empty entry is reached (if check_empty is true)
	 *  3. an entry with the same key is found
	 */
	while(index < PORTMAP_SIZE &&
			portmap->map[index].port_in != PORTMAP_DEFAULT_VALUE &&
			portmap->map[index].port_in != key) {
		index++;
		i++;
	}

	/* if a good entry has been found, we can return it directly */
	if(index < PORTMAP_SIZE)
		return index;

	/* otherwise, we loop back from the beginning */
	index = 0;

	/* we go through the map until either:
	 *  1. i == PORTMAP_SIZE (the whole map has been explored)
	 *  2. an empty entry is reached
	 *  3. an entry with the same key is found
	 */
	while(i < PORTMAP_SIZE &&
			portmap->map[index].port_in != PORTMAP_DEFAULT_VALUE &&
			portmap->map[index].port_in != key) {
		index++;
		i++;
	}

	if(i < PORTMAP_SIZE)
		/* a good entry has been found */
		return index;
	else
		/* the map is full */
		return PORTMAP_SIZE;
}

/**
 * Add an entry to the port map by either finding an available entry,
 * or write on an existing one with the same key.
 * Return true if successful, or false otherwise.
 */
int add_entry(PortMap *portmap, uint16_t port_in, uint16_t port_out)
{
	Tracee *tracee = TRACEE(global_portmap_extension);
	uint16_t index = get_index(portmap, port_in);

	/* no available entry has been found */
	if(index == PORTMAP_SIZE)
		return -1;

	portmap->map[index].port_in = port_in;
	portmap->map[index].port_out = port_out;

	VERBOSE(tracee, PORTMAP_VERBOSITY, "new port mapping entry: %d -> %d", ntohs(port_in), ntohs(port_out));

	return 0;
}

/**
 * Find the entry corresponding to port_in,
 * and returns the associated port_out.
 * If no entry is found, return PORTMAP_DEFAULT_VALUE.
 */
uint16_t get_port(PortMap *portmap, uint16_t port_in)
{
	uint16_t index = get_index(portmap, port_in);

	/* no corresponding entry has been found */
	if(index == PORTMAP_SIZE)
		return PORTMAP_DEFAULT_VALUE;

	if(portmap->map[index].port_in == port_in)
		return portmap->map[index].port_out;
	else
		return PORTMAP_DEFAULT_VALUE;
}
