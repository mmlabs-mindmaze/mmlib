/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmsysio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static
int check_open_files(int argc, char* argv[])
{
	int i;
	int num_fd;
	char line[64];

	for (i = 0; i < argc; i++)
		fprintf(stderr, "%s\n", argv[i]);

	num_fd = atoi(argv[argc-1]);
	fprintf(stderr, "num_fd = %i\n", num_fd);
	for (i = 3; i < 2*num_fd+3; i++) {
		sprintf(line, "fd = %i", i);
		if (mm_write(i, line, strlen(line)) < 0)
			fprintf(stderr, "failed write for fd=%i\n", i);
	}

	return 0;
}


int main(int argc, char* argv[])
{
	if (argc < 2) {
		fprintf(stderr, "child-proc: Missing argument\n");
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "check-open-files") == 0)
		return check_open_files(argc, argv);

	if (strcmp(argv[1], "check-exit") == 0)
		return atoi(argv[2]);

	fprintf(stderr, "child-proc: invalid argument: %s\n", argv[1]);
	return EXIT_FAILURE;
}
