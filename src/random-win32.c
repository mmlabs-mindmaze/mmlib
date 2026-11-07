/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "random-internal.h"

#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>


LOCAL_SYMBOL
void get_random(void* buffer, size_t sz)
{
	NTSTATUS res;

	res = BCryptGenRandom(NULL, buffer, sz, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	// This should never happen
	if (!BCRYPT_SUCCESS(res))
		abort();
}
