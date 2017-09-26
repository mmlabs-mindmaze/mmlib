/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmsysio.h>
#include <mmerrno.h>
#include <stdlib.h>

#define TEST_FILE	"test.dat"

int main(void)
{
	int fd;
	ssize_t rsz;
	char str[] = "Hello world!";

	fd = mm_open(TEST_FILE, O_CREAT|O_TRUNC|O_RDWR, S_IWUSR|S_IRUSR);
	if (fd == -1)
		goto failure;

	rsz = mm_write(fd, str, sizeof(str));
	if (rsz < (ssize_t)sizeof(str))
		goto failure;

	if (mm_unlink(TEST_FILE))
		goto failure;

	if (mm_close(fd))
		goto failure;

	return EXIT_SUCCESS;

failure:
	mm_print_lasterror(NULL);
	return EXIT_FAILURE;
}
