#include <unistd.h>

int check = 0;

static void __attribute__((constructor)) init(void)
{
	if (check > 0)
		_exit(1);

	check++;
}

int main(void)
{
	return 0;
}
