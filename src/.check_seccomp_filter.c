#include <sys/prctl.h>     /* prctl(2), PR_* */
#include <linux/seccomp.h> /* SECCOMP_MODE_FILTER, */
#include <linux/filter.h>  /* struct sock_*, */
#include <linux/audit.h>   /* AUDIT_ARCH_*, */
#include <stddef.h>        /* offsetof(3), */

int main(void)
{
	const size_t arch_offset    = offsetof(struct seccomp_data, arch);
	const size_t syscall_offset = offsetof(struct seccomp_data, nr);
	struct sock_fprog program;

	#define ARCH_NR AUDIT_ARCH_X86_64

	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, arch_offset),
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, AUDIT_ARCH_X86_64, 0, 1),
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, syscall_offset),
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_TRACE)
	};

	program.filter = filter;
	program.len = sizeof(filter) / sizeof(struct sock_filter);

	(void) prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	(void) prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program);

	return 1;
}

