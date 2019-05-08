/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "file-internal.h"
#include "mmerrno.h"
#include "mmlib.h"
#include "mmlog.h"
#include "mmpredefs.h"
#include "mmthread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#  include "utils-win32.h"

#  define getenv getenv_utf8
#  define setenv setenv_utf8
#  define unsetenv unsetenv_utf8

#else //!_WIN32

extern char** environ;

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
const char* mm_getenv(const char* name, const char* default_value)
{
	const char* value = getenv(name);
	return (value) ? value : default_value;
}

#ifdef _WIN32
#define MM_ENV_DELIM ";"
#else
#define MM_ENV_DELIM ":"
#endif
/**
 * mm_setenv() - Add or change environment variable
 * @name:       name of the environment variable
 * @value:      value to set the environment variable called @name
 * @action:     set to 0 if only add is permitted
 *
 * This updates or adds a variable in the environment of the calling
 * process. The @name argument points to a string containing the name of an
 * environment variable to be added or altered. The environment variable
 * will be set to the value to which @value points. If the environment
 * variable named by @name already exists, behavior is determined by the value
 * of the @action variable.
 *
 * If the value of @action is 0, the function shall return success and the
 * environment shall remain unchanged.
 * If the value of @action is 1/MM_ENV_OVERWRITE, the function overwrites the
 * environment and returns.
 * If the value of @action is 2/MM_ENV_PREPEND, the function prepends the
 * @value to the environment.
 * If the value of @action is 3/MM_ENV_APPEND, the function appends the @value
 * to the environment.
 *
 * Note: the PREPEND and APPEND actions only make sense when used with PATH-like
 * environment variables
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_setenv(const char* name, const char* value, int action)
{
	int rv;
	const char * old_value;
	char * new_value = NULL;

	if (action < 0 || action >= MM_ENV_MAX)
		return mm_raise_error(EINVAL, "invalid action value");

	if (action == MM_ENV_PREPEND || action == MM_ENV_APPEND) {
		old_value = mm_getenv(name, NULL);
		if (old_value != NULL) {
			new_value = mm_malloca(strlen(value) + 1 + strlen(old_value));
			if (action == MM_ENV_PREPEND)
				sprintf(new_value, "%s%s%s", value, MM_ENV_DELIM, old_value);
			else  /* action == MM_ENV_APPEND */
				sprintf(new_value, "%s%s%s", old_value, MM_ENV_DELIM, value);

			value = new_value;
		}
	}

	rv = setenv(name, value, action);
	mm_freea(new_value);

	if (rv != 0)
		return mm_raise_from_errno("setenv(%s, %s) failed", name, value);
	else
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


/**
 * mm_get_environ() - get array of environment variables
 *
 * This function gets an NULL-terminated array of strings corresponding to
 * the environment of the current process. Each string has the format
 * "key=val" where key is the name of the environment variable and val its
 * value.
 *
 * NOTE: The array and its content is valid until the environment is
 * modified, ie until any environment variable is added, modified or
 * removed.
 *
 * Return: a NULL terminated array of "key=val" environment strings.
 */
API_EXPORTED
char const* const* mm_get_environ(void)
{
	char** envp;

#if _WIN32
	envp = get_environ_utf8();
#else
	envp = environ;
#endif

	return (char const* const*) envp;
}


/**************************************************************************
 *                                                                        *
 *                      Base directories utils                            *
 *                                                                        *
 **************************************************************************/

static char* basedirs[MM_NUM_DIRTYPE];


MM_DESTRUCTOR(basedir)
{
	int i;

	for (i = 0; i < MM_NELEM(basedirs); i++) {
		free(basedirs[i]);
		basedirs[i] = NULL;
	}
}


/**
 * set_basedir() - assign a value for a known folder
 * @dirtype:    base folder type to assign
 * @value:      value to copy and assign to specified base folder
 */
static
void set_basedir(enum mm_known_dir dirtype, const char* value)
{
	size_t len = strlen(value)+1;
	char* dir;

	dir = malloc(len);
	mm_check(dir != NULL);

	memcpy(dir, value, len);
	basedirs[dirtype] = dir;
}


/**
 * set_dir_or_home_relative() - set a known folder or use default value
 * @dirtype:    base folder type to assign
 * @dir:        value to copy and assign to base folder if not NULL
 * @parent:     parent folder of the default value (if @dir is NULL)
 * @suffix:     suffix to add to @parent to form default if @dir is NULL
 *
 * This function sets basedirs[@dirtype] to a copy of @dir if @dir is not
 * NULL, otherwise basedirs[@dirtype] is set to a path formed by
 * @parent/@suffix
 */
static
void set_dir_or_home_relative(enum mm_known_dir dirtype, const char* dir,
                              const char* parent, const char* suffix)
{
	size_t len;
	char* value;

	if (dir) {
		set_basedir(dirtype, dir);
		return;
	}

	len = strlen(parent) + strlen(suffix) + 2;
	value = mm_malloca(len);
	mm_check(value);

	sprintf(value, "%s/%s", parent, suffix);
	set_basedir(dirtype, value);

	mm_freea(value);
}



static
void init_basedirs(void)
{
	const char *home, *dir;

	home =  mm_getenv("HOME", mm_getenv("USERPROFILE", NULL));
	mm_check(home != NULL, "Surprising: Home folder not specified in env");
	set_basedir(MM_HOME, home);

	dir = mm_getenv("XDG_CONFIG_HOME", NULL);
	set_dir_or_home_relative(MM_CONFIG_HOME, dir, home, ".config");

	dir = mm_getenv("XDG_CACHE_HOME", NULL);
	set_dir_or_home_relative(MM_CACHE_HOME, dir, home, ".cache");

	dir = mm_getenv("XDG_DATA_HOME", NULL);
	set_dir_or_home_relative(MM_DATA_HOME, dir, home, ".local/share");

	dir = mm_getenv("XDG_RUNTIME_DIR", NULL);
	set_dir_or_home_relative(MM_RUNTIME_DIR, dir,
	                         mm_getenv("TEMP", "/tmp"),
	                         mm_getenv("USERNAME", "self"));
}


/**
 * mm_get_basedir() - get location of standard base folder
 * @dirtype:    &enum mm_known_dir value specifying the base folder to get
 *
 * The base folder provided follow the XDG base directory specification
 * https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html.
 * As such, the reported folder can be controlled through environment variables.
 *
 * Return: pointer to the corresponding string in case of success, NULL
 * otherwise with error state set accordingly. If @dirtype is valid, the
 * function cannot fail.
 */
API_EXPORTED
const char* mm_get_basedir(enum mm_known_dir dirtype)
{
	static mmthr_once_t basedirs_once = MMTHR_ONCE_INIT;

	if (dirtype < 0 || dirtype >= MM_NUM_DIRTYPE) {
		mm_raise_error(EINVAL, "Unknown dir type (%i)", dirtype);
		return NULL;
	}

	// Init base directory the first time we reach here
	mmthr_once(&basedirs_once, init_basedirs);

	return basedirs[dirtype];
}


/**
 * mm_path_from_basedir() - form a path using a standard base folder
 * @dirtype:    &enum mm_known_dir value specifying the base folder
 * @suffix:     path to append to base folder
 *
 * Return: pointer to allocated string containing the specified path in case of
 * success, NULL with error state set accordingly otherwise. The allocated string
 * must be freed by a call to free() when it is no longer needed.
 */
API_EXPORTED
char* mm_path_from_basedir(enum mm_known_dir dirtype, const char* suffix)
{
	const char* base;
	char* dir;

	if (!suffix) {
		mm_raise_error(EINVAL, "suffix cannot be null");
		return NULL;
	}

	base = mm_get_basedir(dirtype);
	if (!base)
		return NULL;

	dir = malloc(strlen(base) + strlen(suffix) + 2);
	if (!dir) {
		mm_raise_from_errno("Alloc failed");
		return NULL;
	}

	sprintf(dir, "%s/%s", base, suffix);
	return dir;
}


/**************************************************************************
 *                                                                        *
 *                       Path manipulation utils                          *
 *                                                                        *
 **************************************************************************/

static
const char* get_last_nonsep_ptr(const char* path)
{
	const char* c = path + strlen(path) - 1;

	// skip trailing path separators
	while (c > path && is_path_separator(*c))
		c--;

	return c;
}


static
const char* get_basename_ptr(const char* path)
{
	const char *c, *lastptr;

	lastptr = get_last_nonsep_ptr(path);

	for (c = lastptr-1; c >= path; c--) {
		if (is_path_separator(*c))
			return (c == lastptr) ? c : c + 1;
	}

	return path;
}


/**
 * mm_basename() - get basename of a path
 * @base:       string buffer receiving the result (may be NULL)
 * @path:       path whose basename must be computed (may be NULL)
 *
 * This function takes the pathname pointed to by @path and write in
 * @base (if not NULL) the final component of the pathname, deleting any
 * trailing path separator characters ('/' on POSIX, '/' and '\\' on
 * Windows). If @base is NULL, the result is not written, only the number
 * of character needed by the resulting string is returned.
 *
 * If path is NULL or "", the resulting basename will be ".";
 *
 * Thr pointer @base and @path need not be different: it is legal to use
 * the same buffer for both (thus realizing an inplace transformation of
 * the path). More generally, @base and @path may overlap. Also contrary to
 * its POSIX equivalent, this function is guaranteed to be reentrant.
 *
 * NOTE: If allocated length for @base is greater or equal to length of
 * @path (including null terminator) with a minimum of 2, then it is
 * guaranteed that the result will not overflow @base.
 *
 * Return: the number of character (excluding null termination) that are
 * written to @base (or would be written if @base is NULL)
 */
API_EXPORTED
int mm_basename(char* base, const char* path)
{
	const char* baseptr;
	const char* lastptr;
	int len;

	if (!path)
		path = "";

	baseptr = get_basename_ptr(path);
	lastptr = get_last_nonsep_ptr(path)+1;

	len = lastptr - baseptr;

	// if len == 0, path was "" or "/" (or "//" or "///" or ...)
	if (len <= 0) {
		len = 1;
		if (path[0] == '\0')
			baseptr = ".";
	}

	if (base) {
		memmove(base, baseptr, len);
		base[len] = '\0';
	}

	return len;
}


/**
 * mm_dirname() - get directory name of a path
 * @dir:        string buffer receiving the result (may be NULL)
 * @path:       path whose dirname must be computed (may be NULL)
 *
 * This takes a pointer to a character string @path that contains a
 * pathname, and write the pathname of the parent directory of that file in
 * the buffer pointed to by @dir (if not NULL). This does not perform
 * pathname resolution; the result will not be affected by whether or not
 * path exists or by its file type. Trailing path separator ('/' on POSIX,
 * '/' and '\\' on Windows) not also leading characters are not counted as
 * part of the path. If @dir is NULL, the result is not written, only the
 * number of character needed by the resulting string is returned.
 *
 * If path is NULL or "", the resulting dirname will be ".";
 *
 * The pointers @dir and @path need not be different: it is legal to use
 * the same buffer for both (thus realizing an inplace transformation of
 * the path). More generally, @dir and @path may overlap. Also contrary to
 * its POSIX equivalent, this function is guaranteed to be reentrant.
 *
 * NOTE: If allocated length for @dir is greater or equal to length of
 * @path (including null terminator) with a minimum of 2, then it is
 * guaranteed that the result will not overflow @dir.
 *
 * Return: the number of character (excluding null termination) that are
 * written to @dir (or would be written if @dir is NULL)
 */
API_EXPORTED
int mm_dirname(char* dir, const char* path)
{
	const char* baseptr;
	const char* lastptr;
	int len;

	if (!path)
		path = "";

	baseptr = get_basename_ptr(path);

	if (baseptr == path) {
		len = 1;
		if (!is_path_separator(*baseptr))
			path = ".";

		goto exit;
	}

	lastptr = baseptr-1;

	// Remove trailing separator
	while (lastptr > path && is_path_separator(*lastptr))
		lastptr--;

	len  = lastptr - path + 1;

exit:
	if (dir) {
		memmove(dir, path, len);
		dir[len] = '\0';
	}

	return len;
}
