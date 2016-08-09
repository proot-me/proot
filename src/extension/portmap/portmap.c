#include <stdint.h>         /* intptr_t, */
#include <stdlib.h>         /* strtoul(3), */
#include <sys/un.h>         /* strncpy */
#include <sys/socket.h>	    /* AF_UNIX, AF_INET */
#include <arpa/inet.h>      /* inet_ntop */

#include "cli/note.h"
#include "extension/extension.h"
#include "tracee/mem.h"     /* read_data */

#define PORTMAP_SIZE 4096  /* must be a power of 2 */
#define PORTMAP_DEFAULT_VALUE 0  /* default value that indicates an unused entry */

/**
 * We use a global variable in order to support multiple port mapping options,
 * otherwise we would have a different extension instance for each (port_in, port_out) pair,
 * which would be a waste of memory and performance.
 * This variable is modified only once, in the INITIALIZATION event.
 */
Extension *global_portmap_extension = NULL;

typedef struct PortMapEntry {
	uint16_t port_in;
	uint16_t port_out;
} PortMapEntry;

typedef struct PortMap {
	PortMapEntry map[PORTMAP_SIZE];
	uint16_t table_mask;
} PortMap;

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
uint16_t get_index(PortMap *portmap, uint16_t key) {
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
	uint16_t index = get_index(portmap, port_in);

	/* no available entry has been found */
	if(index == PORTMAP_SIZE)
		return -1;

	portmap->map[index].port_in = port_in;
	portmap->map[index].port_out = port_out;

	note(NULL, INFO, INTERNAL, "new port map entry: %d -> %d", htons(port_in), htons(port_out));

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

typedef struct Config {
	PortMap portmap;
} Config;


#define LOCALHOST_IN_ADDR_T 16777343

/**
 * Change the port of the socket address, if it maps with an entry.
 * Return 0 if no relevant entry is found, and 1 if the port has been changed.
 */
int change_inet_socket_port(Tracee *tracee, Config *config, struct sockaddr_in *sockaddr) {
	uint16_t port_in, port_out;

	port_in = sockaddr->sin_port;
	port_out = get_port(&config->portmap, port_in);

	if(port_out == PORTMAP_DEFAULT_VALUE) {
		note(tracee, INFO, INTERNAL, "port ignored: %d ", htons(port_in));
		return 0;
	}

	sockaddr->sin_port = port_out;
	note(tracee, INFO, INTERNAL, "port translation: %d -> %d", htons(port_in), htons(port_out));

	return 1;
}

static int handle_sysenter_end(Tracee *tracee, Config *config)
{
	int status;
	int sysnum;

	sysnum = get_sysnum(tracee, ORIGINAL);

	switch(sysnum) {
	  case PR_connect:
	  case PR_bind: {
		struct sockaddr_un sockaddr;
		int size;
		word_t sock_addr;

		/* Get the reg address of the socket, and the size of the structure*/
		sock_addr = peek_reg(tracee, CURRENT, SYSARG_2);
		size = (int) peek_reg(tracee, CURRENT, SYSARG_3);
		if (sock_addr == 0)
			return 0;

		/* Essential step, we clean the structure before adding data to it,
		 * otherwise we will have a hell of a time with char arrays filled with rubbish */
		bzero(&sockaddr, sizeof(struct sockaddr_un));

		/* Next, we read the socket address structure from the tracee's memory */
		status = read_data(tracee, &sockaddr, sock_addr, size);

		if (status < 0)
			return status;

		//TODO: Remove comments?
		/* Here is where Unix Domain Socket and IP Socket are dealt with separately.
		 * Although there is still a little detail concerning IP Socket connections:
		 * when an IP socket binds/connects to a (HOST, PORT) couple,
		 * PRoot will actually receive 3 bind/connect system calls (not one!):
		 *
		 * 1) A first one in the AF_FILE domain. What's tricky is that AF_FILE and AF_UNIX
		 * have the same value, so these syscall will still be intercepted by the first 'if' below.
		 * But because their paths do not start with '\0', they will simply be ignored.
		 * Their path is always '/var/run/nscd/socket' by the way.
		 * The NSCD is the Name Service Cache Daemon.
		 *
		 * 2) A second one identical to the first.
		 *
		 * 3) And finally, a bind/connect system call in the domain AF_INET,
		 * that will be captured by the second 'if'.
		 */

		status = 0;
		if (sockaddr.sun_family == AF_INET) {
			status = change_inet_socket_port(tracee, config, (struct sockaddr_in *) &sockaddr);
		}

		if(status <= 0) {
			/* the socket has been ignored, or an error occured */
			return status;
		}

		/* we allocate a new memory place for the modified socket address */
		sock_addr = alloc_mem(tracee, sizeof(sockaddr));
		if (sock_addr == 0)
			return -EFAULT;

		/* we write the modified socket in this new address */
		status = write_data(tracee, sock_addr, &sockaddr, sizeof(struct sockaddr_un));
		if (status < 0)
			return status;

		/* then we modify the syscall argument so that it uses the modified socket address */
		poke_reg(tracee, SYSARG_2, sock_addr);
		poke_reg(tracee, SYSARG_3, sizeof(struct sockaddr_un));

		return 0;
	  }

	  default:
		return 0;
	}
}

/* List of syscalls handled by this extensions.  */
static FilteredSysnum filtered_sysnums[] = {
	{ PR_bind,         0 },
	{ PR_connect,      0 },
	FILTERED_SYSNUM_END,
};

int add_portmap_entry(uint16_t port_in, uint16_t port_out) {
	if(global_portmap_extension == NULL)
		return 0;
	else {
		Config *config = talloc_get_type_abort(global_portmap_extension->config, Config);
		/* careful with little/big endian numbers */
		return add_entry(&config->portmap, ntohs(port_in), ntohs(port_out));
	}
}

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occured.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int portmap_callback(Extension *extension, ExtensionEvent event,
		     intptr_t data1 UNUSED, intptr_t data2 UNUSED)
{
	switch (event) {
	  case INITIALIZATION: {
		Config *config;

		if(global_portmap_extension != NULL)
			return -1;

		extension->config = talloc_zero(extension, Config);
		if (extension->config == NULL)
			return -1;

		config = talloc_get_type_abort(extension->config, Config);
		initialize_portmap(&config->portmap);

		extension->filtered_sysnums = filtered_sysnums;

		global_portmap_extension = extension;
		return 0;
	  }

	  case SYSCALL_ENTER_END: {
		/* As PRoot only translate unix sockets,
		 * it doesn't actually matter whether we do this
		 * on the ENTER_START or ENTER_END. */
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);
		return handle_sysenter_end(tracee, config);
	  }

	  case INHERIT_PARENT: {
		/* Shared configuration with the parent,
		 * as port maps do not change from tracee to tracee. */
		return 0;
	  }

	  default:
		return 0;
	}
}
