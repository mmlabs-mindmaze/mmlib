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
#include <errno.h>


static
int status_to_error_code(NTSTATUS status)
{
	switch (status) {
	case STATUS_INVALID_PARAMETER: return EINVAL;
	case STATUS_INVALID_HANDLE:    return EBADF;
	default:                       return EIO;
	}
}


LOCAL_SYMBOL
int get_random(void* buffer, size_t sz)
{
	NTSTATUS res;

	res = BCryptGenRandom(NULL, buffer, sz, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (!BCRYPT_SUCCESS(res)) {
		_set_errno(status_to_error_code(res));
		return -1;
	}

	return 0;
}
