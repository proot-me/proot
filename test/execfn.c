#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/prctl.h>

#ifndef PR_GET_AUXV
#define PR_GET_AUXV 0x41555856
#endif

/* Print the name AT_EXECFN points to, read through the route given as
 * argument: getauxval(3), which reads the vector on the stack, or the
 * kernel's copy of it through prctl(PR_GET_AUXV) or /proc/self/auxv.
 * The file is read through stdio, which does more than open(2) and
 * read(2).  Exit with 125 if the kernel has no PR_GET_AUXV (< 6.4).  */
int main(int argc, char *argv[])
{
    unsigned long vector[512];
    size_t length;
    size_t i;
    FILE *file;
    int size;

    if (argc != 2)
	return 1;

    if (strcmp(argv[1], "getauxval") == 0) {
	const char *execfn = (const char *) getauxval(AT_EXECFN);

	if (execfn == NULL)
	    return 1;
	puts(execfn);
	return 0;
    }

    if (strcmp(argv[1], "prctl") == 0) {
	size = prctl(PR_GET_AUXV, vector, sizeof(vector), 0, 0);
	if (size < 0)
	    return 125;
	length = ((size_t) size < sizeof(vector) ? (size_t) size
		  : sizeof(vector));
    } else if (strcmp(argv[1], "file") == 0) {
	file = fopen("/proc/self/auxv", "r");
	if (file == NULL)
	    return 1;
	length = fread(vector, 1, sizeof(vector), file);
	fclose(file);
    } else
	return 1;

    for (i = 0; i + 1 < length / sizeof(vector[0]); i += 2) {
	if (vector[i] == AT_NULL)
	    break;
	if (vector[i] == AT_EXECFN) {
	    puts((const char *) vector[i + 1]);
	    return 0;
	}
    }

    return 1;
}
