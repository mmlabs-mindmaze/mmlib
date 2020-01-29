#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "lib_handmaid.h"
#include "mmsysio.h"

int main (void)
{
	char buf = 'a';

	// indicate to its father that the process started
	if (mm_write(1, &buf, sizeof(char)) != 1)
		return -1;

	// call the shared library
	fnct();

	// wait for the father to stop the process
	if (mm_read(0, &buf, sizeof(char)) == -1)
		return -1;

	return 0;
}
