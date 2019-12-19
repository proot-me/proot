#include <linux/auxvec.h> /* AT_*, */
#include <stdio.h>        /* printf(3), */
#include <sys/types.h>    /* open(2), */
#include <sys/stat.h>     /* open(2), */
#include <fcntl.h>        /* open(2), */
#include <unistd.h>       /* read(2), close(2), */
#include <stdlib.h>       /* exit(3), EXIT_*, realloc(3), free(3), */

struct auxv {
	long type;
	long value;
} __attribute__((packed));

void print_auxv(struct auxv *auxv)
{
#define CASE(a)							\
	case (a):						\
		printf("%s = 0x%lx\n", #a, auxv->value);	\
		break;

	switch (auxv->type) {
	CASE(AT_NULL)
	CASE(AT_IGNORE)
	CASE(AT_EXECFD)
	CASE(AT_PHDR)
	CASE(AT_PHENT)
	CASE(AT_PHNUM)
	CASE(AT_PAGESZ)
	CASE(AT_BASE)
	CASE(AT_FLAGS)
	CASE(AT_ENTRY)
	CASE(AT_NOTELF)
	CASE(AT_UID)
	CASE(AT_EUID)
	CASE(AT_GID)
	CASE(AT_EGID)
	CASE(AT_PLATFORM)
	CASE(AT_HWCAP)
	CASE(AT_CLKTCK)
	CASE(AT_SECURE)
	CASE(AT_BASE_PLATFORM)
	CASE(AT_RANDOM)
#if defined(AT_HWCAP2)
	CASE(AT_HWCAP2)
#endif
	CASE(AT_EXECFN)
#if defined(AT_SYSINFO)
	CASE(AT_SYSINFO)
#endif
#if defined(AT_SYSINFO_EHDR)
	CASE(AT_SYSINFO_EHDR)
#endif
	default:
		printf("unknown (%ld) = 0x%lx\n", auxv->type, auxv->value);
		break;
	}

#undef CASE
}

extern char **environ;

int main()
{
	long at_base_proc = 0;
	long at_base_mem = 0;
	struct auxv *auxv;
	void *data = NULL;
	size_t size = 0;
	void **pointer;
	int status;
	int fd;

	for (pointer = (void **) environ; *pointer != NULL; pointer++)
		/* Nothing */;

	for (auxv = (void *) ++pointer; auxv->type != AT_NULL; auxv++) {
		if (auxv->type == AT_BASE)
			at_base_mem = auxv->value;

		print_auxv(auxv);
	}

	printf("----------------------------------------------------------------------\n");

	fd = open("/proc/self/auxv", O_RDONLY);
	if (fd < 0)
		exit(EXIT_FAILURE);

#define CHUNK_SIZE 1024

	do {
		data = realloc(data, size + CHUNK_SIZE);
		if (data == NULL)
			exit(EXIT_FAILURE);

		status = read(fd, data + size, CHUNK_SIZE);
		size += CHUNK_SIZE;
	} while (status > 0);

	for (auxv = data; auxv->type != AT_NULL; auxv++) {
		if (auxv->type == AT_BASE)
			at_base_proc = auxv->value;

		print_auxv(auxv);
	}

	(void) close(fd);
	(void) free(data);

	exit((at_base_proc != 0 && at_base_mem == at_base_proc)
	     ? EXIT_SUCCESS
	     : EXIT_FAILURE);
}
