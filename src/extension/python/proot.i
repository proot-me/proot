%module proot
%{
#include "syscall/sysnum.h"
#include "tracee/tracee.h"
#include "extension/extension.h"
#include "tracee/reg.h"
#include "tracee/mem.h"

extern Tracee *get_tracee_from_extension(long extension);
%}

/* python extension helper */
Tracee *get_tracee_from_extension(long extension);

/* List of possible events.  */
typedef enum {
	GUEST_PATH,
	HOST_PATH,
	SYSCALL_ENTER_START,
	SYSCALL_ENTER_END,
	SYSCALL_EXIT_START,
	SYSCALL_EXIT_END,
	NEW_STATUS,
	INHERIT_PARENT,
	INHERIT_CHILD,
	SYSCALL_CHAINED_ENTER,
	SYSCALL_CHAINED_EXIT,
	INITIALIZATION,
	REMOVED,
	PRINT_CONFIG,
	PRINT_USAGE,
} ExtensionEvent;

typedef enum {
	CURRENT  = 0,
	ORIGINAL = 1,
	MODIFIED = 2,
	NB_REG_VERSION
} RegVersion;


%include "syscall/sysnum.h"
%include "tracee/reg.h"
%include "tracee/mem.h"
