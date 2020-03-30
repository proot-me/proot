%module proot
%{
#define SWIG_FILE_WITH_INIT

#include "arch.h"
#include "syscall/sysnum.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "extension/extension.h"

/* define an internal global with correct PR number */
#define SYSNUM(item) static const int PR_internal ## item = PR_ ## item;
#include "syscall/sysnums.list"
#undef SYSNUM
%}

/* now say PR_item has value PR_internal */
/* works but ugly. Another way to do this ? */
#define SYSNUM(item) static const int PR_ ## item = PR_internal ## item;
%include "syscall/sysnums.list"
#undef SYSNUM

/* python extension helper */
%inline %{
Tracee *get_tracee_from_extension(long extension_handle)
{
	Extension *extension = (Extension *)extension_handle;
	Tracee *tracee = TRACEE(extension);

	return tracee;
}
%}

/* arch.h */
typedef unsigned long word_t;

/* tracee/tracee.h */
typedef enum {
	CURRENT  = 0,
	ORIGINAL = 1,
	MODIFIED = 2,
	NB_REG_VERSION
} RegVersion;

/* syscall/sysnum.h */
typedef enum Sysnum;
extern Sysnum get_sysnum(const Tracee *tracee, RegVersion version);
extern void set_sysnum(Tracee *tracee, Sysnum sysnum);

/* tracee/reg.h */
typedef enum {
	SYSARG_NUM = 0,
	SYSARG_1,
	SYSARG_2,
	SYSARG_3,
	SYSARG_4,
	SYSARG_5,
	SYSARG_6,
	SYSARG_RESULT,
	STACK_POINTER,
	INSTR_POINTER,
	RTLD_FINI,
	STATE_FLAGS,
	USERARG_1,
} Reg;

extern word_t peek_reg(const Tracee *tracee, RegVersion version, Reg reg);
extern void poke_reg(Tracee *tracee, Reg reg, word_t value);

/* tracee/mem.h */
/* make read_data / write_data pythonic */
%apply (char *STRING, size_t LENGTH) { (const void *src_tracer, word_t size2) };
extern int write_data(const Tracee *tracee, word_t dest_tracee, const void *src_tracer, word_t size2);

 %include <cstring.i>
%rename(read_data) read_data_for_python;
%cstring_output_withsize(void *dest_tracer, int *size2);
%inline %{
void read_data_for_python(const Tracee *tracee, word_t src_tracee, void *dest_tracer, int *size2)
{
	int res = read_data(tracee, dest_tracer, src_tracee, *size2);
	/* in case of error we return empty string */
	if (res)
		*size2 = 0;
}
%}

/* extension/extention.h */
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
