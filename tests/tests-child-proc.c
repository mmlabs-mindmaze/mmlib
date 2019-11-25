/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmdlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "mmerrno.h"
#include "mmlog.h"
#include "mmsysio.h"
#include "socket-testlib.h"
#include "tests-child-proc.h"
#include "threaddata-manipulation.h"

#define PASSED_FD 3

int main(int argc, char* argv[])
{
	void* map = NULL;
	int fd;
	unsigned int mapsz;
	int exitcode;
	void *hndl = NULL;
	union {
		void* ptr;
		intptr_t (*fn)(void*);
	} symbol;

	// If 3 argument looks like "mapfile-%i-%u", it means that the caller
	// want us to map the file and size as specified in the argument
	if (argc >= 3 && sscanf(argv[2], "mapfile-%i-%u", &fd, &mapsz) == 2) {
		map = mm_mapfile(fd, 0, mapsz, MM_MAP_RDWR|MM_MAP_SHARED);
		if (!map) {
			mm_print_lasterror("mm_mapfile(%i, %u) failed", fd,
			                   mapsz);
			return EXIT_FAILURE;
		}
	}

	exitcode = EXIT_SUCCESS;

	if (argc < 2) {
		fprintf(stderr, "Missing argument\n");
		exitcode = EXIT_FAILURE;
		goto exit;
	} else if (!strcmp(argv[1], "run_socket_client")) {
		run_socket_client(WR_PIPE_FD, RD_PIPE_FD, argc-2, argv+2);
	} else {
		exitcode = EXIT_FAILURE;

		hndl = mm_dlopen(NULL, MMLD_LAZY);
		if (hndl == NULL)
			goto exit;

		/* Use union to allow cast between func pointer and void* */
		symbol.ptr = mm_dlsym(hndl, argv[1]);
		if (symbol.ptr == NULL) {
			fprintf(stderr, "Unknown arg: %s\n", argv[1]);
			exitcode = EXIT_FAILURE;
			goto exit;
		}

		fprintf(stderr, "Running: %s\n", argv[1]);
		/* function should return NULL on success */
		exitcode = symbol.fn(map);
		mmlog_debug("%s exited: %d", argv[1], exitcode);
	}

exit:
	mm_dlclose(hndl);
	mm_unmap(map);
	return exitcode;
}
