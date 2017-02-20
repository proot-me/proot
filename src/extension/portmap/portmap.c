#include <stdint.h>         /* intptr_t, */
#include <stdlib.h>         /* strtoul(3), */
#include <sys/un.h>         /* strncpy */
#include <sys/socket.h>	    /* AF_UNIX, AF_INET */
#include <arpa/inet.h>      /* inet_ntop */

#include "cli/note.h"
#include "extension/extension.h"
#include "tracee/mem.h"     /* read_data */
#include "syscall/chain.h"  /* register_chained_syscall */

#define PORTMAP_SIZE 4096  /* must be a power of 2 */
#define PORTMAP_DEFAULT_VALUE 0  /* default value that indicates an unused entry */
#define PORTMAP_VERBOSITY 1

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
	Tracee *tracee = TRACEE(global_portmap_extension);

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

typedef struct Config {
	PortMap portmap;
	bool netcoop_mode;
	bool need_to_check_new_port;
	uint16_t old_port;
	word_t sockfd;
} Config;


int valid_port_to_change(uint16_t port) {
	return htons(port) > 1024;
}

/**
 * Change the port of the socket address, if it maps with an entry.
 * Return 0 if no relevant entry is found, and 1 if the port has been changed.
 */
int change_inet_socket_port(Tracee *tracee, Config *config, struct sockaddr_in *sockaddr, bool bind_mode) {
	uint16_t port_in, port_out;

	port_in = sockaddr->sin_port;
	port_out = get_port(&config->portmap, port_in);

	if(port_out == PORTMAP_DEFAULT_VALUE) {
		if (bind_mode && config->netcoop_mode && !config->need_to_check_new_port && valid_port_to_change(port_in)) {
			VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv4 netcoop mode with: %d", htons(port_in));
			sockaddr->sin_port = 0; // the system will assign an available port
			config->old_port = port_in; // we keep this one for adding a new entry
			config->need_to_check_new_port = true;
			config->sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
			return 1;
		}

		VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv4 port ignored: %d ", htons(port_in));
		return 0;
	}

	sockaddr->sin_port = port_out;
	VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv4 port translation: %d -> %d", htons(port_in), htons(port_out));

	return 1;
}

/**
 * Change the port of the socket address, if it maps with an entry.
 * Return 0 if no relevant entry is found, and 1 if the port has been changed.
 */
int change_inet6_socket_port(Tracee *tracee, Config *config, struct sockaddr_in6 *sockaddr, bool bind_mode) {
	uint16_t port_in, port_out;

	port_in = sockaddr->sin6_port;
	port_out = get_port(&config->portmap, port_in);

	if(port_out == PORTMAP_DEFAULT_VALUE) {
		if (bind_mode && config->netcoop_mode && !config->need_to_check_new_port && valid_port_to_change(port_in)) {
			VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv6 netcoop mode with: %d", htons(port_in));
			sockaddr->sin6_port = 0; // the system will assign an available port
			config->old_port = port_in; // we keep this one for adding a new entry
			config->need_to_check_new_port = true;
			config->sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
			return 1;
		}

		VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv6 port ignored: %d ", htons(port_in));
		return 0;
	}

	sockaddr->sin6_port = port_out;
	VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv6 port translation: %d -> %d", htons(port_in), htons(port_out));

	return 1;
}

int prepare_getsockname_chained_syscall(Tracee *tracee, Config *config) {
	int status = 0;
	word_t sockfd, sock_addr, size_addr;
	struct sockaddr_un sockaddr;
	socklen_t size;

	/* we retrieve this one from the listen() system call */
	sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
	size = sizeof(sockaddr);

	/* we check that it's the correct socket */
	if(sockfd != config->sockfd)
		return 0;

	/* we allocate a memory place to store the socket address.
	 * This is a buffer that will be filled by the getsockname() syscall.
	 */
	sock_addr = alloc_mem(tracee, sizeof(sockaddr));
	if (sock_addr == 0)
		return -EFAULT;
	size_addr = alloc_mem(tracee, sizeof(socklen_t));
	if(size_addr == 0)
		return -EFAULT;

	bzero(&sockaddr, sizeof(sockaddr));

	/* we write the modified socket in this new address */
	status = write_data(tracee, sock_addr, &sockaddr, sizeof(sockaddr));
	if (status < 0)
		return status;
	status = write_data(tracee, size_addr, &size, sizeof(size));
	if (status < 0)
		return status;

	/* Only by using getsockname can we retrieve the port automatically
	 * assigned by the system to the socket.
	 */
	status = register_chained_syscall(
			tracee, PR_getsockname,
			sockfd,  // SYS_ARG1, socket file descriptor.
			sock_addr,
			size_addr,
			0, 0, 0
			);
	if (status < 0)
		return status;
	//status = restart_original_syscall(tracee);
	//if (status < 0)
	//    return status;

	sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
	return 0;
}


static int handle_sysenter_end(Tracee *tracee, Config *config)
{
	int status;
	int sysnum;

	sysnum = get_sysnum(tracee, CURRENT);

	switch(sysnum) {
	  case PR_connect:
	  case PR_bind:
	  {
		struct sockaddr_un sockaddr;
		int size;
		word_t sock_addr;

		/* Get the reg address of the socket, and the size of the structure.
		 * Note that the sockaddr and addrlen are at the same position for all 4 of these syscalls.
		 */
		sock_addr = peek_reg(tracee, CURRENT, SYSARG_2);
		size = (int) peek_reg(tracee, CURRENT, SYSARG_3);

		if (sock_addr == 0)
			return 0;

		/* Essential step, we clean the structure before adding data to it */
		bzero(&sockaddr, sizeof(sockaddr));

		/* Next, we read the socket address structure from the tracee's memory */
		status = read_data(tracee, &sockaddr, sock_addr, sizeof(sockaddr));

		if (status < 0)
			return status;

		//if(sysnum == PR_connect || sysnum == PR_bind) {
		/* Before binding to a socket, the system does some connect() to the NSCD (Name Service Cache Daemon) on its own.
		 * These connect calls are for sockets in the AF_FILE domain; remember that AF_FILE and AF_UNIX have the same value.
		 * Their path is always '/var/run/nscd/socket'.
		 */

		status = 0;
		if (sockaddr.sun_family == AF_INET) {
			status = change_inet_socket_port(tracee, config, (struct sockaddr_in *) &sockaddr, sysnum == PR_bind);
		}
		else if (sockaddr.sun_family == AF_INET6) {
			status = change_inet6_socket_port(tracee, config, (struct sockaddr_in6 *) &sockaddr, sysnum == PR_bind);
		}

		if(status <= 0) {
			/* the socket has been ignored, or an error occured */
			return status;
		}

		/* we allocate a new memory place for the modified socket address */
		sock_addr = alloc_mem(tracee, size);
		if (sock_addr == 0)
			return -EFAULT;

		/* we write the modified socket in this new address */
		status = write_data(tracee, sock_addr, &sockaddr, sizeof(sockaddr));
		if (status < 0)
			return status;

		/* then we modify the syscall argument so that it uses the modified socket address */
		poke_reg(tracee, SYSARG_2, sock_addr);
		poke_reg(tracee, SYSARG_3, size);

		return 0;
	  }

	  case PR_listen: {
		if(!config->netcoop_mode || !config->need_to_check_new_port)
			return 0;

		status = prepare_getsockname_chained_syscall(tracee, config);
		return status;
	  }

	  default:
			  return 0;
	}
}

int add_changed_port_as_entry(Tracee *tracee, Config *config) {
	int status;
	struct sockaddr_un sockaddr;
	word_t sockfd, sock_addr;
	struct sockaddr_in *sockaddr_in;
	struct sockaddr_in6 *sockaddr_in6;
	int result;
	uint16_t port_in, port_out;

	if(!config->need_to_check_new_port)
		return 0;

	sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
	sock_addr = peek_reg(tracee, CURRENT, SYSARG_2);
	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);

	if (sock_addr == 0)
		return 0;

	if (sockfd != config->sockfd)
		return 0;

	if (result < 0)
		return -result;

	/* Essential step, we clean the structure before adding data to it */
	bzero(&sockaddr, sizeof(sockaddr));

	/* Next, we read the socket address structure from the tracee's memory */
	status = read_data(tracee, &sockaddr, sock_addr, sizeof(sockaddr));
	if (status < 0)
		return status;

	port_in = config->old_port;

	if (sockaddr.sun_family == AF_INET) {
		sockaddr_in = (struct sockaddr_in *) &sockaddr;
		port_out = sockaddr_in->sin_port;
	}
	else if (sockaddr.sun_family == AF_INET6) {
		sockaddr_in6 = (struct sockaddr_in6 *) &sockaddr;
		port_out = sockaddr_in6->sin6_port;
	}
	else
		return 0;

	add_portmap_entry(htons(port_in), htons(port_out));
	config->need_to_check_new_port = false;
	config->sockfd = 0;

	return 0;
}

static int handle_syschained_exit(Tracee *tracee, Config *config)
{
	int sysnum;

	sysnum = get_sysnum(tracee, CURRENT);

	switch(sysnum) {
	  case PR_getsockname: {
		return add_changed_port_as_entry(tracee, config);
			       }
	  default:
			       return 0;
	}
}

/* List of syscalls handled by this extension.  */
static FilteredSysnum filtered_sysnums[] = {
	{ PR_bind,         0 },
	{ PR_connect,      0 },
	{ PR_listen,       FILTER_SYSEXIT },  /* the exit stage is required to chain syscalls */
//      { PR_getsockname,  FILTER_SYSEXIT },  /* not needed here, see CHAINED EXIT event */
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

int activate_netcoop_mode() {
	if(global_portmap_extension != NULL) {
		Config *config = talloc_get_type_abort(global_portmap_extension->config, Config);
		config->netcoop_mode = true;
	}

	return 0;
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
		config->netcoop_mode = false;
		config->need_to_check_new_port = false;
		config->sockfd = 0;

		extension->filtered_sysnums = filtered_sysnums;

		global_portmap_extension = extension;
		return 0;
	  }

	  case SYSCALL_ENTER_END: {
		/* As PRoot only translate unix sockets,
		 * it doesn't actually matter whether we do this
		 * on the ENTER_START or ENTER_END stage. */
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);
		return handle_sysenter_end(tracee, config);
	  }

	  case SYSCALL_CHAINED_EXIT: {
		Tracee *tracee = TRACEE(extension);
		Config *config = talloc_get_type_abort(extension->config, Config);
		return handle_syschained_exit(tracee, config);
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
