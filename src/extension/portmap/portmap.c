/*
 * Copyright (C) 2016 Vincent Hage
 */

#include <stdint.h>         /* intptr_t, */
#include <stdlib.h>         /* strtoul(3), */
#include <sys/un.h>         /* strncpy */
#include <sys/socket.h>	    /* AF_UNIX, AF_INET */
#include <arpa/inet.h>      /* inet_ntop */
#include <linux/net.h>   	/* SYS_*, */

#include "cli/note.h"
#include "extension/extension.h"
#include "tracee/mem.h"     /* read_data */
#include "syscall/chain.h"  /* register_chained_syscall */
#include "extension/portmap/portmap.h"

Extension *global_portmap_extension = NULL;

typedef struct Config {
	PortMap portmap;
	bool netcoop_mode;
	bool need_to_check_new_port;
	uint16_t old_port;
	word_t sockfd;
} Config;

/**
 * Change the port of the socket address, if it maps with an entry.
 * Return 0 if no relevant entry is found, and 1 if the port has been changed.
 */
int change_inet_socket_port(Tracee *tracee, Config *config, word_t sockfd, struct sockaddr_in *sockaddr, bool bind_mode) {
	uint16_t port_in, port_out;

	port_in = sockaddr->sin_port;
	port_out = get_port(&config->portmap, port_in);

	if(port_out == PORTMAP_DEFAULT_VALUE) {
		if (bind_mode && config->netcoop_mode && !config->need_to_check_new_port) {
			VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv4 netcoop mode with: %d", htons(port_in));
			sockaddr->sin_port = 0; // the system will assign an available port
			config->old_port = port_in; // we keep this one for adding a new entry
			config->need_to_check_new_port = true;
			config->sockfd = sockfd;
			return 1;
		}

		VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv4 port ignored: %d ", htons(port_in));
		return 0;
	}

	sockaddr->sin_port = port_out;
	VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv4 port translation: %d -> %d (NOT GUARANTEED: bind might still fail on target port)", htons(port_in), htons(port_out));

	return 1;
}

/**
 * Change the port of the socket address, if it maps with an entry.
 * Return 0 if no relevant entry is found, and 1 if the port has been changed.
 */
int change_inet6_socket_port(Tracee *tracee, Config *config, word_t sockfd, struct sockaddr_in6 *sockaddr, bool bind_mode) {
	uint16_t port_in, port_out;

	port_in = sockaddr->sin6_port;
	port_out = get_port(&config->portmap, port_in);

	if(port_out == PORTMAP_DEFAULT_VALUE) {
		if (bind_mode && config->netcoop_mode && !config->need_to_check_new_port) {
			VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv6 netcoop mode with: %d", htons(port_in));
			sockaddr->sin6_port = 0; // the system will assign an available port
			config->old_port = port_in; // we keep this one for adding a new entry
			config->need_to_check_new_port = true;
			config->sockfd = sockfd;
			return 1;
		}

		VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv6 port ignored: %d ", htons(port_in));
		return 0;
	}

	sockaddr->sin6_port = port_out;
	VERBOSE(tracee, PORTMAP_VERBOSITY, "ipv6 port translation: %d -> %d (NOT GUARANTEED: bind might still fail on target port)", htons(port_in), htons(port_out));

	return 1;
}

int prepare_getsockname_chained_syscall(Tracee *tracee, Config *config, word_t sockfd, int is_socketcall) {
	int status = 0;
	word_t sock_addr, size_addr;
	struct sockaddr_un sockaddr;
	socklen_t size;

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
	if (!is_socketcall) {
		status = register_chained_syscall(
				tracee, PR_getsockname,
				sockfd,  // SYS_ARG1, socket file descriptor.
				sock_addr,
				size_addr,
				0, 0, 0
		);
	} else {
		unsigned long args[6];
		
		args[0] = sockfd;
		args[1] = sock_addr;
		args[2] = size_addr;
		args[3] = 0;
		args[4] = 0;
		args[5] = 0;
		
		/* We allocate a little bloc of memory to store socketcall's arguments */
		word_t args_addr = alloc_mem(tracee, 6 * sizeof_word(tracee));
		
		status = write_data(tracee, args_addr, &args, 6 * sizeof_word(tracee));
	
		status = register_chained_syscall(
				tracee, PR_socketcall,
				SYS_GETSOCKNAME,
				args_addr,  // SYS_ARG1, socket file descriptor.
				0,
				0,
				0, 0
		);
	}
			
	if (status < 0)
		return status;
	//status = restart_original_syscall(tracee);
	//if (status < 0)
	//    return status;

	return 0;
}

int translate_port(Tracee *tracee, Config *config, word_t sockfd, word_t *sock_addr, int size, int is_bind_syscall) {
	struct sockaddr_un sockaddr;
	int status;
	
	if (sock_addr == 0)
		return 0;
		
	/* Essential step, we clean the structure before adding data to it */
	bzero(&sockaddr, sizeof(sockaddr));

	/* Next, we read the socket address structure from the tracee's memory */
	status = read_data(tracee, &sockaddr, *sock_addr, size);

	if (status < 0)
		return status;

	//if(sysnum == PR_connect || sysnum == PR_bind) {
	/* Before binding to a socket, the system does some connect() 
	 * to the NSCD (Name Service Cache Daemon) on its own.
	 * These connect calls are for sockets in the AF_FILE domain; 
	 * remember that AF_FILE and AF_UNIX have the same value.
	 * Their path is always '/var/run/nscd/socket'.
	*/

	status = 0;
	if (sockaddr.sun_family == AF_INET) {
		status = change_inet_socket_port(tracee, config, sockfd, (struct sockaddr_in *) &sockaddr, is_bind_syscall);
	}
	else if (sockaddr.sun_family == AF_INET6) {
		status = change_inet6_socket_port(tracee, config, sockfd, (struct sockaddr_in6 *) &sockaddr, is_bind_syscall);
	}

	if (status <= 0) {
		/* the socket has been ignored, or an error occured */
		return status;
	}

	/* we allocate a new memory place for the modified socket address */
	*sock_addr = alloc_mem(tracee, sizeof(sockaddr));
	if (sock_addr == 0)
		return -EFAULT;

	/* we write the modified socket in this new address */
	status = write_data(tracee, *sock_addr, &sockaddr, sizeof(sockaddr));
	if (status < 0)
		return status;
		
	return 0;
}

static int handle_sysenter_end(Tracee *tracee, Config *config)
{
	int status;
	int sysnum;

	sysnum = get_sysnum(tracee, CURRENT);

	switch(sysnum) {
#define SYSARG_ADDR(n) (args_addr + ((n) - 1) * sizeof_word(tracee))

#define PEEK_WORD(addr, forced_errno)		\
	peek_word(tracee, addr);		\
	if (errno != 0) {			\
		status = forced_errno ?: -errno; \
		break;				\
	}

#define POKE_WORD(addr, value)			\
	poke_word(tracee, addr, value);		\
	if (errno != 0) {			\
		status = -errno;		\
		break;				\
	}
	
	case PR_socketcall:	{
		word_t sockfd;
		word_t args_addr;
		word_t sock_addr_saved;
		word_t sock_addr;
		word_t size;
		word_t call;
		int is_bind_syscall = sysnum == PR_bind;
		
		call = peek_reg(tracee, CURRENT, SYSARG_1);
		is_bind_syscall = call == SYS_BIND;
		args_addr = peek_reg(tracee, CURRENT, SYSARG_2);
	
		switch(call) {
		case SYS_BIND:
		case SYS_CONNECT: {
			/* Remember: PEEK_WORD puts -errno in status and breaks if an
			 * error occured.  */
			sockfd = PEEK_WORD(SYSARG_ADDR(1), 0);
			sock_addr = PEEK_WORD(SYSARG_ADDR(2), 0);
			size      = PEEK_WORD(SYSARG_ADDR(3), 0);

			sock_addr_saved = sock_addr;
			status = translate_port(tracee, config, sockfd, &sock_addr, size, is_bind_syscall);
			if (status < 0)
				break;

			/* These parameters are used/restored at the exit stage.  */
			poke_reg(tracee, SYSARG_5, sock_addr_saved);
			poke_reg(tracee, SYSARG_6, size);

			/* Remember: POKE_WORD puts -errno in status and breaks if an
			 * error occured.  */
			POKE_WORD(SYSARG_ADDR(2), sock_addr);
			POKE_WORD(SYSARG_ADDR(3), sizeof(struct sockaddr_un));
			return 0;
		}
		case SYS_LISTEN: {
			word_t sockfd;
		
			if(!config->netcoop_mode || !config->need_to_check_new_port)
				return 0;

			/* we retrieve this one from the listen() system call */
			sockfd = PEEK_WORD(SYSARG_ADDR(1), 0);
	
			status = prepare_getsockname_chained_syscall(tracee, config, sockfd, true);
			
			return status;
		}
		default:
			return 0;
		}
		break;
	}
	
#undef SYSARG_ADDR
#undef PEEK_WORD
#undef POKE_WORD

	case PR_connect:
	case PR_bind: {
		int size;
		int is_bind_syscall;
		word_t sockfd, sock_addr;

		/* 
		* Get the reg address of the socket, and the size of the structure.
		* Note that the sockaddr and addrlen are at the same position for all 4 of these syscalls.
		*/
		sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
		sock_addr = peek_reg(tracee, CURRENT, SYSARG_2);
		size = (int) peek_reg(tracee, CURRENT, SYSARG_3);
		is_bind_syscall = sysnum == PR_bind;

		status = translate_port(tracee, config, sockfd, &sock_addr, size, is_bind_syscall);
		if (status < 0) {
			return status;
		}

		/* then we modify the syscall argument so that it uses the modified socket address */
		poke_reg(tracee, SYSARG_2, sock_addr);
		//poke_reg(tracee, SYSARG_3, size);
		poke_reg(tracee, SYSARG_3, sizeof(struct sockaddr_un));

		return 0;
	}
	case PR_listen: {
		word_t sockfd;
		
		if(!config->netcoop_mode || !config->need_to_check_new_port)
			return 0;

		/* we retrieve this one from the listen() system call */
		sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
			
		status = prepare_getsockname_chained_syscall(tracee, config, sockfd, false);
		return status;
	}
	default:
		return 0;
	}
	
	return 0;
}

int add_changed_port_as_entry(Tracee *tracee, Config *config, word_t sockfd, word_t sock_addr, int result) {
	int status;
	struct sockaddr_un sockaddr;
	struct sockaddr_in *sockaddr_in;
	struct sockaddr_in6 *sockaddr_in6;
	uint16_t port_in, port_out;

	if(!config->need_to_check_new_port)
		return 0;


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
		
#define SYSARG_ADDR(n) (args_addr + ((n) - 1) * sizeof_word(tracee))

#define PEEK_WORD(addr, forced_errno)		\
	peek_word(tracee, addr);		\
	if (errno != 0) {			\
		status = forced_errno ?: -errno; \
		break;				\
	}

#define POKE_WORD(addr, value)			\
	poke_word(tracee, addr, value);		\
	if (errno != 0) {			\
		status = -errno;		\
		break;				\
	}
	
	case PR_socketcall:	{
		word_t args_addr;
		word_t call;

		call = peek_reg(tracee, CURRENT, SYSARG_1);
		args_addr = peek_reg(tracee, CURRENT, SYSARG_2);
	
		switch(call) {
			case SYS_GETSOCKNAME:{
				word_t sockfd, sock_addr;
				int result, status;
			
				sockfd = PEEK_WORD(SYSARG_ADDR(1), 0);
				sock_addr = PEEK_WORD(SYSARG_ADDR(2), 0);
				result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
		
				status = add_changed_port_as_entry(tracee, config, sockfd, sock_addr, result);
				return status;
			}
			default:
				return 0;
		}
		return 0;
	}
	
#undef SYSARG_ADDR
#undef PEEK_WORD
#undef POKE_WORD

	case PR_getsockname:{
		word_t sockfd, sock_addr;
		int result;
	
		sockfd = peek_reg(tracee, CURRENT, SYSARG_1);
		sock_addr = peek_reg(tracee, CURRENT, SYSARG_2);
		result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	
		return add_changed_port_as_entry(tracee, config, sockfd, sock_addr, result);
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
	{ PR_socketcall,   0 }, /* for x86 processors with kernel < 4.3 */
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
