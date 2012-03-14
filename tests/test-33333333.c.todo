/* Check a child is traced even if its parent doesn't call wait(2).
 * 
 * Reported-by: Clément BAZIN <clement.bazin@st.com>
 *              on Ubuntu 11.10 x86_64
 */

#include <stdlib.h> /* exit(3), */
#include <unistd.h> /* fork(2), */

int main(void)
{
	switch (fork()) {
	case -1:
		exit(EXIT_FAILURE);

	case 0: /* Child: XXX */
		sleep(2);
		return 0;

	default: /* Parent: "look child, no wait(2)!" */
		return 1;
	}
}
