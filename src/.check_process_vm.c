#include <sys/uio.h>
#include <stdlib.h>

int main(void)
{
	return process_vm_readv(0, NULL, 0, NULL, 0, 0)
	       + process_vm_writev(0, NULL, 0, NULL, 0, 0);
}
