/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "random-internal.h"
#include "mmpredefs.h"

#include <sys/random.h>
#include <stdlib.h>


/**
 * get_random() - obtain a series of random bytes
 * @buffer:     pointer to buffer receiving the random data
 * @sz:         size of @buffer
 */
LOCAL_SYMBOL
void get_random(void* buffer, size_t sz)
{
	ssize_t rsz;
	char* cbuf = buffer;

	do {
		rsz = getrandom(cbuf, sz, 0);
		if (UNLIKELY(rsz < 0))
			abort();

		sz -= rsz;
		cbuf += rsz;
	} while (sz);
}
