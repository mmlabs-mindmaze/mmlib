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

#  include "utils-win32.h"

#  define getenv getenv_utf8
#  define setenv setenv_utf8
#  define unsetenv unsetenv_utf8

#endif


/**
 * mm_getenv() - Return environment variable or default value
 * @name:          name of the environment variable
 * @default_value: default value
 *
 * Return: the value set in the environment if the variable @name is
 * set. Otherwise @default_value is returned.
 */
API_EXPORTED
char* mm_getenv(const char* name, char* default_value)
{
	char* value = getenv(name);
	return (value) ? value : default_value;
}


/**
 * mm_setenv() - Add or change environment variable
 * @name:       name of the environment variable
 * @value:      value to set the environment variable called @name
 * @overwrite:  set to 0 if only add is permitted
 *
 * This updates or adds a variable in the environment of the calling
 * process. The @name argument points to a string containing the name of an
 * environment variable to be added or altered. The environment variable
 * will be set to the value to which @value points. If the environment
 * variable named by @name already exists and the value of overwrite is
 * non-zero, the function return success and the environment is updated. If
 * the environment variable named by @name already exists and the value of
 * @overwrite is zero, the function shall return success and the environment
 * shall remain unchanged.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_setenv(const char* name, char* value, int overwrite)
{
	if (setenv(name,  value, overwrite))
		mm_raise_from_errno("setenv(%s, %s) failed", name, value);

	return 0;
}


/**
 * mm_unsetenv() - remove an environment variable
 * @name:       name of environment variable to remove
 *
 * This function removes an environment variable from the environment of the
 * calling process. The @name argument points to a string, which is the name
 * of the variable to be removed. If the named variable does not exist in
 * the current environment, the environment shall be unchanged and the
 * function is considered to have completed successfully.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_unsetenv(const char* name)
{
	if (unsetenv(name))
		mm_raise_from_errno("unsetenv(%s) failed", name);

	return 0;
}
