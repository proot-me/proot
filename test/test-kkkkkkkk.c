#include <unistd.h>       /* syscall(2), sysconf(3), */
#include <sys/syscall.h>  /* SYS_brk,         */
#include <stdio.h>        /* puts(3),         */
#include <stdlib.h>       /* exit(3), EXIT_*, */
#include <stdint.h>       /* uint*_t,         */
#include <sys/mman.h>     /* mmap(2), MAP_*,  */
#include <string.h>       /* memset(3), */

int main()
{
	int exit_status = EXIT_SUCCESS;
	uint8_t *current_brk = 0;
	uint8_t *initial_brk;
	uint8_t *new_brk;
	uint8_t *old_brk;
	int failure = 0;
	long page_size;
	int i;

	page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size <= 0)
		return 125;

	void test_brk(int increment, int expected_result) {
		new_brk = (uint8_t *) syscall(SYS_brk, current_brk + increment);
		if ((new_brk == current_brk) == expected_result)
			failure = 1;
		current_brk = (uint8_t *) syscall(SYS_brk, 0);
	}

	void test_result() {
		if (!failure)
			puts("OK");
		else {
			puts("failure");
			exit_status = EXIT_FAILURE;
		}
	}

	void test_title(const char *title) {
		failure = 0;
		printf("%-45s : ", title);
		fflush(stdout);
	}

	test_title("Initialization");
	test_brk(0, 1);
	initial_brk = current_brk;
	test_result();

	test_title("Don't set the \"brk\" below its initial value");
	test_brk(page_size, 1);
	// Try to shrink the size of the heap by more than it's current size
	// and it should fail.
	test_brk(-1000 * page_size, 0);
	test_brk(-page_size, 1);
	test_result();

	test_title("Don't overlap \"brk\" pages");
	test_brk(page_size, 1);
	test_brk(page_size, 1);
	test_result();

	/* Preparation for the test "Re-allocated heap is initialized".  */
	old_brk = current_brk - page_size;
	memset(old_brk, 0xFF, page_size);

	test_title("Don't allocate the same \"brk\" page twice");
	test_brk(-page_size, 1);
	test_brk(page_size, 1);
	test_result();

	test_title("Re-allocated \"brk\" pages are initialized");
	for (i = 0; i < page_size; i++) {
		if (old_brk[i] != 0) {
			printf("(index = %d, value = 0x%x) ", i, old_brk[i]);
			failure = 1;
			break;
		}
	}
	test_result();

#if 0
	test_title("Don't allocate \"brk\" pages over \"mmap\" pages");
	new_brk = mmap(current_brk, page_size / 2, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (new_brk == (void *) -1)
		puts("unknown");
	else {
		test_brk(page_size, 0);
		test_result();
	}
#endif

	test_title("All \"brk\" pages are writable (please wait)");
#if 0
	if (munmap(current_brk, page_size / 2) != 0)
		puts("unknown");
	else {
#endif
		while (current_brk - initial_brk < 512*1024*1024UL) {
			old_brk = current_brk;

			test_brk(page_size, -1);
			if (old_brk == current_brk)
				break;

			for (i = 0; i < page_size; i++)
				old_brk[i] = 0xAA;
		}
		test_result();
#if 0
	}
#endif

	test_title("Maximum size of the heap >= 1MB");
	failure = (current_brk - initial_brk) < 1024 * 1024;
	test_result();

	test_title("All \"brk\" pages are cleared (please wait)");
	test_brk(initial_brk - current_brk, 1);
	if (current_brk != initial_brk)
		puts("unknown");
	else {
		while (current_brk - initial_brk < 1024*1024*1024UL) {
			old_brk = current_brk;

			test_brk(3 * page_size, -1);
			if (old_brk == current_brk)
				break;

			for (i = 0; i < 3 * page_size; i++) {
				if (old_brk[i] != 0) {
					printf("(index = %d, value = 0x%x) ", i, old_brk[i]);
					failure = 1;
					goto end;
				}
			}
		}
	end:
		test_result();
	}

	exit(exit_status);
}
