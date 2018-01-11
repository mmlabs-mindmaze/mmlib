/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "threaddata-manipulation.h"
#include "socket-testlib.h"
#include "tests-child-proc.h"
#include "mmsysio.h"
#include "mmerrno.h"
#include <stdlib.h>
#include <stdio.h>

#define PASSED_FD       3


int main(int argc, char* argv[])
{
	void* map = NULL;
	int fd;
	unsigned int mapsz;
	int exitcode;

	// If 3 argument looks like "mapfile-%i-%u", it means that the caller
	// want us to map the file and size as specified in the argument
	if (argc >= 3 && sscanf(argv[2], "mapfile-%i-%u", &fd, &mapsz) == 2) {
		map = mm_mapfile(fd, 0, mapsz, MM_MAP_RDWR|MM_MAP_SHARED);
		if (!map) {
			mm_print_lasterror("mm_mapfile(%i, %u) failed", fd, mapsz);
			return EXIT_FAILURE;
		}
	}

	exitcode = EXIT_SUCCESS;

	if (argc < 2) {
		fprintf(stderr, "Missing argument\n");
		exitcode = EXIT_FAILURE;
	} else if (!strcmp(argv[1], "run_write_shared_data")) {
		run_write_shared_data(map);
	} else if (!strcmp(argv[1], "run_notif_data")) {
		run_notif_data(map);
	} else if (!strcmp(argv[1], "run_robust_mutex_write_data")) {
		run_robust_mutex_write_data(map);
	} else if (!strcmp(argv[1], "run_socket_client")) {
		run_socket_client(WR_PIPE_FD, RD_PIPE_FD, argc-2, argv+2);
	} else {
		fprintf(stderr, "Unknown arg: %s\n", argv[1]);
		exitcode = EXIT_FAILURE;
	}

	mm_unmap(map);
	return exitcode;
}
