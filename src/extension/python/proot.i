%module proot
%{
#include "syscall/sysnum.h"
#include "tracee/tracee.h"
extern Tracee *get_tracee_from_extension(long extension);
%}

typedef enum {
	PR_void = 0,
} Sysnum;

typedef enum {
	CURRENT  = 0,
	ORIGINAL = 1,
	MODIFIED = 2,
	NB_REG_VERSION
} RegVersion;

Tracee *get_tracee_from_extension(long extension);

Sysnum get_sysnum(Tracee *tracee, RegVersion version);
