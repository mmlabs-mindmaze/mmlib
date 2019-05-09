/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmlib.h"
#include "utils-win32.h"
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <errno.h>

/**************************************************************************
 *                                                                        *
 *                     Environment strings cache                          *
 *                                                                        *
 **************************************************************************/

/**
 * struct envstr - environment string entry
 * @str:        buffer holder the env string (of the form "name=value")
 * @namelen:    length of the name string (ie, offset of '=' character)
 * @str_maxlen: allocated size in @str
 */
struct envstr {
	char* str;
	int namelen;
	int str_maxlen;
};


/**
 * struct envcache - structure holding all encountered UTF-8 env strings
 * @max_arrlen: allocated number of element in @array
 * @arrlen:     number of element used in @array
 * @array:      buffer of environment strings
 * @envp:       cache of NULL terminated array to environment strings
 */
struct envcache {
	int max_arrlen;
	int arrlen;
	struct envstr* array;
	char** envp;
};


/**
 * envstr_init() - environment string initialization
 * @entry:      env string to initialize
 * @name:       name of the environment variable in UTF-8
 *
 * Return: 0 in case of success, -1 with errno set otherwise
 */
static
int envstr_init(struct envstr* entry, const char* name)
{
	int str_maxlen, namelen;

	// Allocate string buffer to be large enough to hold variable name
	// and decent value
	namelen = strlen(name);
	str_maxlen = namelen + 64;
	entry->str = malloc(str_maxlen);
	if (!entry->str)
		return -1;

	// Initialize the rest of the fields
	entry->namelen = namelen;
	entry->str_maxlen = str_maxlen;

	// Initialize string buffer with "name=", the value part of the
	// string will be set later
	memcpy(entry->str, name, namelen);
	entry->str[namelen] = '=';
	entry->str[namelen+1] = '\0';

	return 0;
}


/**
 * envstr_deinit() - cleanup environment string
 * @entry:      environment string to cleanup (may be NULL)
 *
 * This function is idempotent
 */
static
void envstr_deinit(struct envstr* entry)
{
	if (!entry)
		return;

	free(entry->str);

	*entry = (struct envstr){.str = NULL};
}


/**
 * envstr_get_value() - get pointer of the value part of environment string
 * @entry:      environment string whose value must be retrieved
 *
 * Return: pointer to the value part in the environment string
 */
static
char* envstr_get_value(struct envstr* entry)
{
	// The value part start right after the '=' after the name
	return entry->str + entry->namelen+1;
}


/**
 * envstr_resize_value() - resize buffer to accommodate a size of value
 * @entry:      environment string whose buffer must be resized if needed
 * @value_len:  size of value (excluding null termination) that must be
 *              accommodated
 *
 * Return: 0 in case of success, -1 with errno set otherwise
 */
static
int envstr_resize_value(struct envstr* entry, int value_len)
{
	char* str;
	int envstr_len;

	// The new size must be large enough for variable name, '=', value
	// and terminating nul character
	envstr_len = value_len + entry->namelen + 2;

	// If the allocated string is not large enough, realloc
	if (entry->str_maxlen < envstr_len) {
		str = realloc(entry->str, envstr_len);
		if (!str)
			return -1;

		entry->str = str;
		entry->str_maxlen = envstr_len;
	}

	return 0;
}


/**
 * envstr_set_value() - set value part of an environment string
 * @entry:      environment string whose value must be modified
 * @value:      new value string to set
 *
 * This function will reallocate the string buffer if necessary to
 * accommodate the new value string
 *
 * Return: 0 in case of success, -1 otherwise with errno set
 */
static
int envstr_set_value(struct envstr* entry, const char* value)
{
	int value_len;

	// Estimate the new size needed and resize string buffer
	value_len = strlen(value);
	if (envstr_resize_value(entry, value_len))
		return -1;

	// Copy the new value in the value part of the string
	memcpy(envstr_get_value(entry), value, value_len+1);

	return 0;
}


/**
 * envcache_deinit() - cleanup the environment cache
 * @cache:	cache to cleanup
 */
static
void envcache_deinit(struct envcache* cache)
{
	int i;

	for (i = 0; i < cache->arrlen; i++)
		envstr_deinit(&cache->array[i]);

	free(cache->envp);
	free(cache->array);
	*cache = (struct envcache){.array = NULL};
}


/**
 * envcache_find_entry() - find the named environment string in the cache
 * @cache:      cache to search
 * @name:       name of the environment variable to search
 */
static
struct envstr* envcache_find_entry(struct envcache* cache, const char* name)
{
	int i, namelen;

	namelen = strlen(name);
	
	for (i = 0; i < cache->arrlen; i++) {
		if (namelen != cache->array[i].namelen)
			continue;

		if (!memcmp(cache->array[i].str, name, namelen))
			return &cache->array[i];
	}

	return NULL;
}


/**
 * envcache_create_entry() - create an entry in the cache for the named env var
 * @cache:      cache in which the environment string must be created
 * @name:       name of the environment variable
 *
 * Return: pointer to the created environment string in case of success,
 * NULL otherwise with errno set
 */
static
struct envstr* envcache_create_entry(struct envcache* cache, const char* name)
{
	int max_arrlen;
	struct envstr *entry, *array;

	array = cache->array;
	max_arrlen = cache->max_arrlen;

	// Resize the array if not large enough to accommodate the new entry
	if (cache->arrlen+1 > max_arrlen) {
		max_arrlen = (max_arrlen == 0) ? 32 : max_arrlen*2;
		array = realloc(array, max_arrlen * sizeof(*array));
		if (!array)
			return NULL;

		cache->array = array;
		cache->max_arrlen = max_arrlen;
	}

	// Get the new entry to use and initialize it
	entry = &array[cache->arrlen++];
	if (envstr_init(entry, name)) {
		cache->arrlen--;
		return NULL;
	}

	return entry;
}


/**
 * envcache_remove_entry() - remove environment string from cache
 * @cache:       environment cache to update
 * @entry:       pointer to environment string to remove (may be NULL)
 */
static
void envcache_remove_entry(struct envcache* cache, struct envstr* entry)
{
	int index;

	if (!entry)
		return;

	envstr_deinit(entry);

	// Remove the entry from the array
	index = entry - cache->array;
	memmove(cache->array + index, cache->array + index + 1,
	        (cache->arrlen - index -1)*sizeof(*entry));

	cache->arrlen--;
}


/**
 * envcache_update_envp() - update environ strings array
 * @cache:       environment cache to update
 *
 * return: the new environ strings array in case of success, NULL otherwise
 */
static
char** envcache_update_envp(struct envcache* cache)
{
	int i;
	char** envp;

	envp = realloc(cache->envp, (cache->arrlen+1)*sizeof(*envp));
	if (!envp)
		return NULL;

	for (i = 0; i < cache->arrlen; i++)
		envp[i] = cache->array[i].str;

	envp[cache->arrlen] = NULL;
	cache->envp = envp;
	return envp;
}


/**************************************************************************
 *                                                                        *
 *                 UTF-8 encoded environment manipulation                 *
 *                                                                        *
 **************************************************************************/

/**
 * get_u16env() - get UTF-16 environment value from UTF-8 name
 * @name_u8:    name of the variable in UTF-8
 *
 * Return: the value in UTF-16 of the environment variable named @name_u8 if
 * present, NULL otherwise.
 */
static
char16_t* get_u16env(const char* name_u8)
{
	char16_t *name_u16, *value_u16;
	int u16_len;

	// Estimate the size of the variable name in UTF-16. If the name in
	// invalid, we consider (rightfully) that the environment does not
	// contains such a name
	u16_len = get_utf16_buffer_len_from_utf8(name_u8);
	if (u16_len  < 0)
		return NULL;

	// Get UTF-16 env value from temporary UTF-16 variable name
	name_u16 = mm_malloca(u16_len * sizeof(*name_u16));
	conv_utf8_to_utf16(name_u16, u16_len, name_u8);
	value_u16 = _wgetenv(name_u16);
	mm_freea(name_u16);

	return value_u16;
}


// global UTF-8 environment strings cache
static struct envcache utf8_env_cache;

// Destructor of the UTF-8 cache. This function is called when the program
// terminates or the library is unloaded (in case of dynamic load)
MM_DESTRUCTOR(utf8_environment_cache_cleanup)
{
	envcache_deinit(&utf8_env_cache);
}


/**
 * update_environment_cache() - update cache from "key=val" string in UTF-16
 * @cache:       environment cache to update
 * @envstr_u16:  "key=val" string in UTF-16
 */
static
void update_environment_cache(struct envcache* cache, const char16_t* envstr_u16)
{
	int len8;
	char *envstr_u8, *name, *value;
	struct envstr* entry;

	// Get temporary UTF-8 env string from envstr_u16.
	len8 = get_utf8_buffer_len_from_utf16(envstr_u16);
	envstr_u8 = mm_malloca(len8);
	conv_utf16_to_utf8(envstr_u8, len8, envstr_u16);

	// Get pointer of name and value (terminate name by '\0')
	name = envstr_u8;
	value = strchr(envstr_u8, '=');
	*value = '\0';
	value++;

	// Create new or get existing entry with name
	entry = envcache_find_entry(cache, name);
	if (!entry) {
		entry = envcache_create_entry(cache, name);
		if (!entry)
			goto exit;
	}

	envstr_set_value(entry, value);

exit:
	mm_freea(envstr_u8);
}


/**
 * get_environ_utf8() - Get array of environment strings in UTF-8
 *
 * This function update the environment cache with all environment variable
 * sets in the process.
 *
 * Return: NULL terminated array of the environment string in UTF-8. In
 * case of failure (memory allocation issues), NULL is returned.
 */
LOCAL_SYMBOL
char** get_environ_utf8(void)
{
	struct envcache* cache = &utf8_env_cache;
	LPWCH env_strw;
	char16_t* env16;

	env_strw = GetEnvironmentStringsW();
	if (env_strw == NULL)
		return NULL;

	// Update the environment cache with all key=value strings present
	// in environment
	for (env16 = env_strw; *env16 != L'\0'; env16 += wcslen(env16)+1)
		update_environment_cache(cache, env16);

	FreeEnvironmentStringsW(env_strw);

	return envcache_update_envp(cache);
}


/**
 * getenv_utf8() - get value in UTF-8 of an environment variable
 * @name:       name (UTF-8 encoded) of the environment variable
 *
 * This function is the same as getenv() of the ISO C excepting it has been
 * made to support UTF-8 on Windows.
 *
 * Return: pointer to UTF-8 string containing the value for the specified
 * name. If the specified name cannot be found in the environment of the
 * calling process, a null pointer is returned.
 */
LOCAL_SYMBOL
char* getenv_utf8(const char* name)
{
	int value_len;
	char16_t* val_u16;
	struct envstr* entry;
	char* value;
	struct envcache* cache = &utf8_env_cache;

	// Fast path: if environment is in cache, just return the cached value
	entry = envcache_find_entry(cache, name);
	if (entry)
		return envstr_get_value(entry);

	// See if the environment exist in CRT, if not, NULL value is
	// returned
	val_u16 = get_u16env(name);
	if (!val_u16)
		return NULL;
	
	// Allocate an entry in the cache
	entry = envcache_create_entry(cache, name);
	if (!entry)
		return NULL;

	// Resize the entry value for the required string size after
	// conversion to UTF-8
	value_len = get_utf8_buffer_len_from_utf16(val_u16);
	if (envstr_resize_value(entry, value_len-1))
		return NULL;

	// Perform the actual UTF-16->UTF-8 conversion of the value string
	value = envstr_get_value(entry);
	conv_utf16_to_utf8(value, value_len, val_u16);

	return value;
}


/**
 * setenv_utf8() - set value in UTF-8 of an environment variable
 * @name:       name (UTF-8 encoded) of the environment variable
 * @value:      UTF-8 encoded value
 * @overwrite:  non-zero if existing variable must be overwritten
 *
 * This function is the same as setenv() of the ISO C excepting it has been
 * made to support UTF-8 on Windows.
 *
 * Return: Upon successful completion, zero is returned. Otherwise, -1 and
 * errno set to indicate the error, and the environment will be unchanged.
 */
LOCAL_SYMBOL
int setenv_utf8(const char* name, const char* value, int overwrite)
{
	struct envstr* entry;
	char16_t* str_u16;
	int name_u16_len, value_u16_len, retval = -1;
	struct envcache* cache = &utf8_env_cache;

	// Estimate the size of components of the environment string in UTF-16
	name_u16_len = get_utf16_buffer_len_from_utf8(name);
	value_u16_len = get_utf16_buffer_len_from_utf8(value);
	if (name_u16_len  <= 0 || value_u16_len < 0) {
		errno = (name_u16_len == 0) ? EILSEQ : EINVAL;
		return -1;
	}

	// Get UTF-16 env value from temporary UTF-16 variable name. After
	// this block, str_u16 contains only the variable name in UTF-16
	str_u16 = mm_malloca((name_u16_len + value_u16_len)*sizeof(*str_u16));
	conv_utf8_to_utf16(str_u16, name_u16_len, name);

	// Skip setting env if the variable exist and overwrite is null
	if (!overwrite && _wgetenv(str_u16)) {
		retval = 0;
		goto exit;
	}

	// Find cache entry or allocate if not found
	if ( !(entry = envcache_find_entry(cache, name))
	  && !(entry = envcache_create_entry(cache, name)) )
		goto exit;

	// Store the UTF-8 value in the cache
	if (envstr_set_value(entry, value))
		goto exit;

	// Complete UTF-16 with "=value" part and push it in CRT env
	// (name_u16_len has terminating nul character)
	str_u16[name_u16_len-1] = L'=';
	conv_utf8_to_utf16(str_u16 + name_u16_len, value_u16_len, value);
	if (_wputenv(str_u16)) {
		envcache_remove_entry(cache, entry);
		goto exit;
	}

	retval = 0;

exit:
	mm_freea(str_u16);
	return retval;
}


/**
 * unsetenv_utf8() - remove an environment variable
 * @name:       UTF-8 encoded name of environment variable to remove
 *
 * This function is the same as unsetenv() of the ISO C excepting it has
 * been made to support UTF-8 on Windows.
 *
 * Return: 0 in case of success, -1 otherwise environment unchanged and
 * errno set accordingly
 */
LOCAL_SYMBOL
int unsetenv_utf8(const char* name)
{
	struct envstr* entry;
	int name_u16_len, retval;
	char16_t* str_u16;
	struct envcache* cache = &utf8_env_cache;

	// Remove matching entry from cache if any
	entry = envcache_find_entry(cache, name);
	envcache_remove_entry(cache, entry);

	// Estimate the size of components of the environment string in UTF-16
	// (name_u16_len has terminating nul character)
	name_u16_len = get_utf16_buffer_len_from_utf8(name);
	if (name_u16_len < 0) {
		errno = EINVAL;
		return -1;
	}

	// Allocate temporary string set to "name=" and push it with
	// _wputenv(). This way, the environment variable named "name" will
	// be removed.
	str_u16 = mm_malloca(name_u16_len+1);
	conv_utf8_to_utf16(str_u16, name_u16_len, name);
	str_u16[name_u16_len-1] = L'=';
	str_u16[name_u16_len] = L'\0';
	retval = _wputenv(str_u16);
	mm_freea(str_u16);

	return retval;
}
