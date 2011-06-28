#define _ATFILE_SOURCE
#include <unistd.h>

int main(void)
{
	char buf[1];
	return readlinkat(0, "", buf, 0);
}
