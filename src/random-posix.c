/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "random-internal.h"

#include <sys/random.h>

#include "mmerrno.h"


/**
 * get_random() - obtain a series of random bytes
 * @buffer:     pointer to buffer receiving the random data
 * @sz:         size of @buffer
 *
 * Returns: 0 in case of success, -1 otherwise with error state set.
 */
LOCAL_SYMBOL
int get_random(void* buffer, size_t sz)
{
	ssize_t rsz;
	char* cbuf = buffer;

	do {
		rsz = getrandom(cbuf, sz, 0);
		if (rsz < 0)
			return mm_raise_from_errno("getrandom() failed");

		sz -= rsz;
		cbuf += rsz;
	} while (sz);

	return 0;
}
