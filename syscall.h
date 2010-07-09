#ifndef SYSCALL_H
#define SYSCALL_H

enum arg_number {
	/* Warning: arg_offset[] in syscall.c relies on this ordering. */
	ARG_SYSNUM = 0,
	ARG_1,
	ARG_2,
	ARG_3,
	ARG_4,
	ARG_5,
	ARG_6,
	ARG_RESULT,

	/* Helpers. */
	ARG_FIRST = ARG_SYSNUM,
	ARG_LAST  = ARG_RESULT
};

extern unsigned long get_syscall_arg(pid_t pid, enum arg_number arg_number);
extern void set_syscall_arg(pid_t pid, enum arg_number arg_number, unsigned long value);

#endif /* SYSCALL_H */
