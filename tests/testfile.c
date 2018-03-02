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
	int fd, dup_fd;
	ssize_t rsz;
	char str[] = "Hello world!";

	fd = mm_open(TEST_FILE, O_CREAT|O_TRUNC|O_RDWR, S_IWUSR|S_IRUSR);
	if (fd == -1)
		goto failure;

	dup_fd = mm_dup(fd);
	if (dup_fd == -1)
		goto failure;

	dup_fd = mm_dup2(fd, dup_fd);
	if (dup_fd == -1)
		goto failure;

	/* Note: dup2() call should close fd */

	rsz = mm_write(dup_fd, str, sizeof(str));
	if (rsz < (ssize_t)sizeof(str))
		goto failure;

	if (mm_unlink(TEST_FILE))
		goto failure;

	if (mm_close(dup_fd))
		goto failure;


	return EXIT_SUCCESS;

failure:
	mm_print_lasterror(NULL);
	return EXIT_FAILURE;
}
