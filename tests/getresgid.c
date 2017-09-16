#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <stdio.h>

int main()
{
	uid_t rid;
	uid_t eid;
	uid_t sid;

	getresgid(&rid, &eid, &sid);
	printf("%d %d %d\n", rid, eid, sid);

	return 0;
}
