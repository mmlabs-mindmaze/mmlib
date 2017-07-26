/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmsysio.h>
#include <mmpredefs.h>
#include <mmerrno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


#define NUM_FILE	3


static
int full_read(int fd, void* buf, size_t len)
{
	char* cbuf = buf;
	ssize_t rsz;

	while (len > 0) {
		rsz = read(fd, cbuf, len);
		if (rsz < 0) {
			perror("failed to read file");
			return -1;
		}

		if (rsz == 0) {
			fprintf(stderr, "EOF reached (missing %i bytes)\n",
			                (unsigned int)len);
			return -1;
		}

		cbuf += rsz;
		len -= rsz;
	}

	return 0;
}

static
int check_expected_fd_content(int num_map, const struct mm_remap_fd* fd_map)
{
	int i;
	int parent_fd, child_fd;
	size_t exp_sz;
	char line[128], expected[128];

	for (i = 0; i < num_map; i++) {
		parent_fd = fd_map[i].parent_fd;
		child_fd = fd_map[i].child_fd;

		exp_sz = sprintf(expected, "fd = %i", child_fd);

		lseek(parent_fd, 0, SEEK_SET);
		if (full_read(parent_fd, line, exp_sz))
			return -1;

		if (memcmp(line, expected, exp_sz)) {
			fprintf(stderr, "failure:\nexpected: %s\ngot: %s\n", expected, line);
			return -1;
		}
	}

	return 0;
}

int main(void)
{
	mm_pid_t pid = 0;
	int i;
	int fds[2*NUM_FILE+3];
	struct mm_remap_fd fd_map[NUM_FILE];
	char filename[MM_NELEM(fds)][32];
	char cmd[] = BUILDDIR"/child-proc"EXEEXT;
	int failure = 0;
	int status;
	char* argv[] = {cmd, "opt1", "another opt2", "Whi opt3", MM_STRINGIFY(NUM_FILE), NULL};

	for (i = 3; i < MM_NELEM(fds); i++) {
		sprintf(filename[i], "file-test-%i", i);
		fds[i] = open(filename[i], O_RDWR|O_TRUNC|O_CREAT, S_IRWXU);
	}

	printf("map_fd = [");
	for (i = 0; i < MM_NELEM(fd_map); i++) {
		fd_map[i].child_fd = i+3;
		fd_map[i].parent_fd = i+3+NUM_FILE;
		printf(" %i:%i", fd_map[i].child_fd, fd_map[i].parent_fd);
	}
	printf(" ]\n");
	fflush(stdout);

	if (mm_spawn(&pid, cmd, MM_NELEM(fd_map), fd_map, 0, argv, NULL)) {
		mm_print_lasterror(NULL);
		failure = 1;
		goto exit;
	}
	mm_wait_process(pid, &status);

exit:
	if (check_expected_fd_content(MM_NELEM(fd_map), fd_map))
		failure = 1;

	for (i = 3; i < MM_NELEM(fds); i++) {
		close(fds[i]);
		unlink(filename[i]);
	}

	return failure ? EXIT_FAILURE : EXIT_SUCCESS;
}
