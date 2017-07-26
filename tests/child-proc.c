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

int main(int argc, char* argv[])
{
	int i;
	int num_fd;
	char line[64];

	for (i = 0; i < argc; i++)
		printf("%s\n", argv[i]);

	num_fd = atoi(argv[argc-1]);
	printf("num_fd = %i\n", num_fd);
	for (i = 3; i < 2*num_fd+3; i++) {
		sprintf(line, "fd = %i", i);
		if (mm_write(i, line, strlen(line)) < 0)
			printf("failed write for fd=%i\n", i);
	}
	fflush(stdout);

	return 0;
}
