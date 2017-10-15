/*
 * Copyright (C) 2016 Vincent Hage
 */

#ifndef PORTMAP_H
#define PORTMAP_H

#include "extension/extension.h"

#define PORTMAP_SIZE 4096  /* must be a power of 2 */
#define PORTMAP_DEFAULT_VALUE 0  /* default value that indicates an unused entry */
#define PORTMAP_VERBOSITY 1

typedef struct PortMapEntry {
	uint16_t port_in;
	uint16_t port_out;
} PortMapEntry;

typedef struct PortMap {
	PortMapEntry map[PORTMAP_SIZE];
	uint16_t table_mask;
} PortMap;

void initialize_portmap(PortMap *portmap);
uint16_t get_index(PortMap *portmap, uint16_t key);
int add_entry(PortMap *portmap, uint16_t port_in, uint16_t port_out);
uint16_t get_port(PortMap *portmap, uint16_t port_in);

int add_portmap_entry(uint16_t port_in, uint16_t port_out);
int activate_netcoop_mode();

#endif /* PORTMAP_H */
