#include "extension/extension.h"
#include "tracee/tracee.h"
#include "syscall/sysnum.h"
#include "arch.h"
#include "attribute.h"

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int link2symlink_callback(Extension *extension, ExtensionEvent event,
				  intptr_t data1 UNUSED, intptr_t data2 UNUSED)
{
	switch (event) {
	case INITIALIZATION: {
		/* List of syscalls handled by this extensions.  */
		static FilteredSysnum filtered_sysnums[] = {
			{ PR_link, FILTER_SYSEXIT },
			{ PR_linkat, FILTER_SYSEXIT },
			FILTERED_SYSNUM_END,
		};
		extension->filtered_sysnums = filtered_sysnums;
		return 0;
	}

	case SYSCALL_ENTER_END: {
		Tracee *tracee = TRACEE(extension);
		
		switch (get_sysnum(tracee, ORIGINAL)) {
		case PR_link:
			/* Convert:
			 *
			 *     int link(const char *oldpath, const char *newpath);
			 *
			 * into:
			 *
			 *     int symlink(const char *oldpath, const char *newpath);
			 */
			set_sysnum(tracee, PR_symlink);
			break;

		case PR_linkat: {
			/* Convert:
			 *
			 *     int linkat(int olddirfd, const char *oldpath,
			 *                int newdirfd, const char *newpath, int flags);
			 *
			 * into:
			 *
			 *     int symlinkat(const char *oldpath, int newdirfd, const char *newpath);
			 *
			 * Note: PRoot has already canonicalized
			 * linkat() paths this way:
			 *
			 *   olddirfd + oldpath -> oldpath
			 *   newdirfd + newpath -> newpath
			 */

			word_t old = peek_reg(tracee, CURRENT, SYSARG_2);
			word_t new = peek_reg(tracee, CURRENT, SYSARG_4);

			poke_reg(tracee, SYSARG_1, old);
			poke_reg(tracee, SYSARG_2, AT_FDCWD);
			poke_reg(tracee, SYSARG_3, new);
			set_sysnum(tracee, PR_symlinkat);
			break;
		}

		default:
			break;
		}
		return 0;
	}

	default:
		return 0;
	}
}
