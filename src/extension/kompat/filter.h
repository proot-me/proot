/* This file was generated thanks to the following command:
 *
 *     grep '^case PR_' enter.c exit.c | cut -f 2 -d ' ' | cut -f 1 -d : | sort -u
 */

{ PR_accept4,		0 },
{ PR_dup3,		0 },
{ PR_epoll_create1,	0 },
{ PR_epoll_pwait, 	0 },
{ PR_eventfd2, 		0 },
{ PR_faccessat, 	0 },
{ PR_fchmodat, 		0 },
{ PR_fchownat, 		0 },
{ PR_fstatat64, 	0 },
{ PR_futimesat, 	0 },
{ PR_inotify_init1, 	0 },
{ PR_linkat, 		0 },
{ PR_mkdirat, 		0 },
{ PR_mknodat, 		0 },
{ PR_newfstatat, 	0 },
{ PR_openat, 		0 },
{ PR_pipe2, 		0 },
{ PR_pselect6, 		0 },
{ PR_readlinkat, 	0 },
{ PR_renameat, 		0 },
{ PR_signalfd4, 	0 },
{ PR_symlinkat, 	0 },
{ PR_uname, 		FILTER_SYSEXIT },
{ PR_unlinkat, 		0 },
