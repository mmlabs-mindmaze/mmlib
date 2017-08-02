/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmlib.h"
#include "mmerrno.h"

#include <stdlib.h>

#ifdef _WIN32

static
int setenv(const char* name, char* value, int overwrite)
{
	int errcode;

	if (!name) {
		errno = EINVAL;
		return -1;
	}

	if (!overwrite && getenv(name))
		return 0;

	errcode = _putenv_s(name, value);
	if (errcode) {
		errno = errcode;
		return -1;
	}

	return 0;
}


static
int unsetenv(const char* name)
{
	int errcode;

	errcode = _putenv_s(name, "");
	if (errcode) {
		errno = errcode;
		return -1;
	}

	return 0;
}

#endif


API_EXPORTED
char* mm_getenv(const char* name, char* default_value)
{
	char* value = getenv(name);
	return (value) ? value : default_value;
}


API_EXPORTED
int mm_setenv(const char* name, char* value, int overwrite)
{
	if (setenv(name,  value, overwrite))
		mm_raise_from_errno("setenv(%s, %s) failed", name, value);

	return 0;
}


API_EXPORTED
int mm_unsetenv(const char* name)
{
	if (unsetenv(name))
		mm_raise_from_errno("unsetenv(%s) failed", name);

	return 0;
}
