/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdio.h>
#include <stdlib.h>

#include "api-testcases.h"

#include "mmerrno.h"
#include "mmlib.h"

#define NUM_ENVVAR 32


START_TEST(get_basedir)
{
	enum mm_known_dir dirtype = _i;

	if (dirtype >= 0 && dirtype < MM_NUM_DIRTYPE) {
		// dirtype is valid, the function must not return NULL
		ck_assert(mm_get_basedir(dirtype) != NULL);
	} else {
		// dirtype is NOT valid, the function report EINVAL
		ck_assert(mm_get_basedir(dirtype) == NULL);
		ck_assert(mm_get_lasterror_number() == EINVAL);
	}
}
END_TEST


START_TEST(path_from_base)
{
	enum mm_known_dir dirtype = _i;
	char* respath;
	char ref[512];
	char* strcase[] = {"a_string", "long/tstring/hello"};
	int i;

	// If dirtype is NOT valid, the function must report EINVAL
	if (dirtype < 0 || dirtype >= MM_NUM_DIRTYPE) {
		ck_assert(mm_path_from_basedir(dirtype, strcase[0]) == NULL);
		ck_assert(mm_get_lasterror_number() == EINVAL);
		return;
	}

	for (i = 0; i < MM_NELEM(strcase); i++) {
		respath = mm_path_from_basedir(dirtype, strcase[i]);
		ck_assert(respath != NULL);

		// Check resulting path is expected
		sprintf(ref, "%s/%s", mm_get_basedir(dirtype), strcase[i]);
		ck_assert_str_eq(ref, respath);

		free(respath);
	}

	// Check function fails with invalid arg
	ck_assert(mm_path_from_basedir(dirtype, NULL) == NULL);
	ck_assert(mm_get_lasterror_number() == EINVAL);
}
END_TEST


START_TEST(mmstrcasecmp_test)
{
	ck_assert(mmstrcasecmp("teststring", "teststring") == 0);
	/* mmstrcasecmp() compares as lower case:
	 * 'S' = 0x53
	 * '_' = 0x5F
	 * 's' = 0x73
	 *
	 * so: 'S' < '_' < 's'
	 *
	 * therefore strcmp() returns "JOHN_HENRY" > "JOHNSTON"
	 * but mmstrcasecmp() returns "JOHN_HENRY" < "JOHNSTON"
	 */
	ck_assert(strcmp("JOHN_HENRY", "JOHNSTON") > 0);
	ck_assert(mmstrcasecmp("JOHN_HENRY", "JOHNSTON") < 0);
}
END_TEST


#define STR_STARTS_WITH(str, const_str) \
	(strlen(str) >= (sizeof(const_str) - 1) \
	 && memcmp(str, const_str, sizeof(const_str) - 1) == 0)

#define STR_ENDS_WITH(str, const_str) \
	(strlen(str) >= (sizeof(const_str) - 1) \
	 && memcmp(str + strlen(str) - sizeof(const_str) + 1, const_str, sizeof(const_str) - 1) == 0)

START_TEST(append_prepend_environ)
{
	// Verify DUMMY_VAR is initially unset
	ck_assert(mm_getenv("DUMMY_VAR", NULL) == NULL);

	// Test get default value is used for getenv if variable is unset
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", "something"), "something");

	// test setenv with overwrite works
	mm_setenv("DUMMY_VAR", "another", MM_ENV_OVERWRITE);
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", NULL), "another");

	// test setenv with prepend flag
	mm_setenv("DUMMY_VAR", "before", MM_ENV_PREPEND);
	ck_assert(STR_STARTS_WITH(mm_getenv("DUMMY_VAR", NULL), "before"));

	// test setenv with append flag
	mm_setenv("DUMMY_VAR", "after", MM_ENV_APPEND);
	ck_assert(STR_ENDS_WITH(mm_getenv("DUMMY_VAR", NULL), "after"));

	// test unsetenv works
	mm_unsetenv("DUMMY_VAR");
	ck_assert(mm_getenv("DUMMY_VAR", NULL) == NULL);

	// test setenv append on empty var
	mm_setenv("DUMMY_VAR", "a val", MM_ENV_APPEND);
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", NULL), "a val");

	// cleaning
	mm_unsetenv("DUMMY_VAR");
	ck_assert(mm_getenv("DUMMY_VAR", NULL) == NULL);
}
END_TEST


START_TEST(get_set_unset_env)
{
	// Verify DUMMY_VAR is initially unset
	ck_assert(mm_getenv("DUMMY_VAR", NULL) == NULL);

	// Test get default value is used for getenv if variable is unset
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", "something"), "something");

	// test setenv without overwrite works
	mm_setenv("DUMMY_VAR", "a val", 0);
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", NULL), "a val");
	mm_setenv("DUMMY_VAR", "another", 0);
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", NULL), "a val");

	// test setenv with overwrite works
	mm_setenv("DUMMY_VAR", "another", 1);
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", NULL), "another");

	// test default value of getenv is not used if variable is set
	ck_assert_str_eq(mm_getenv("DUMMY_VAR", "something"), "another");

	// test unsetenv works
	mm_unsetenv("DUMMY_VAR");
	ck_assert(mm_getenv("DUMMY_VAR", NULL) == NULL);
}
END_TEST


static
const char* get_value_from_envp(const char* const* envp, const char* key)
{
	int i, keylen;

	keylen = strlen(key);

	for (i = 0; envp[i] != NULL; i++) {
		if (  memcmp(key, envp[i], keylen) == 0
		   && envp[i][keylen] == '=')
			return envp[i] + keylen + 1;
	}

	return NULL;
}


START_TEST(get_environ)
{
	const char* const* envp;

	// Verify DUMMY_VAR is initially unset
	ck_assert(mm_getenv("DUMMY_VAR", NULL) == NULL);

	// Check return envp does not contains DUMMY_VAR
	envp = mm_get_environ();
	ck_assert(envp != NULL);
	ck_assert(get_value_from_envp(envp, "DUMMY_VAR") == NULL);

	// Check return envp contains DUMMY_VAR set to value set
	mm_setenv("DUMMY_VAR", "a_val", 0);
	envp = mm_get_environ();
	ck_assert(envp != NULL);
	ck_assert_str_eq(get_value_from_envp(envp, "DUMMY_VAR"), "a_val");

	// Check return envp contains DUMMY_VAR set to new value set
	mm_setenv("DUMMY_VAR", "another", 1);
	envp = mm_get_environ();
	ck_assert(envp != NULL);
	ck_assert_str_eq(get_value_from_envp(envp, "DUMMY_VAR"), "another");

	// Check return envp contains no DUMMY_VAR after unset
	mm_unsetenv("DUMMY_VAR");
	envp = mm_get_environ();
	ck_assert(envp != NULL);
	ck_assert(get_value_from_envp(envp, "DUMMY_VAR") == NULL);
}
END_TEST


/*
 * Instead of setting only one variable and see its value over different
 * manipulation, here we ensure that setting/unsetting multiple variable does
 * not have buggy side effect on the other variables.
 */
START_TEST(multiple_set_unset_env)
{
	char name[NUM_ENVVAR][16], value[NUM_ENVVAR][16];
	const char* const* envp;
	const char* val;
	int i;

	// set multiple variable first
	for (i = 0; i < NUM_ENVVAR; i++) {
		sprintf(name[i], "ENV-KEY-%i", i);
		sprintf(value[i], "VAL-%i", i);
		ck_assert(mm_setenv(name[i], value[i], 0) == 0);
	}

	// unset 1 envvar out of 3
	for (i = 0; i < NUM_ENVVAR; i++) {
		if (i % 3 == 0)
			mm_unsetenv(name[i]);
	}

	// check expected state of the envvar
	for (i = 0; i < NUM_ENVVAR; i++) {
		val = mm_getenv(name[i], NULL);
		if (i % 3 == 0)
			ck_assert(val == NULL);
		else
			ck_assert_str_eq(val, value[i]);
	}

	// check expected state through mm_get_environ()
	envp = mm_get_environ();
	for (i = 0; i < NUM_ENVVAR; i++) {
		val = get_value_from_envp(envp, name[i]);
		if (i % 3 == 0)
			ck_assert(val == NULL);
		else
			ck_assert_str_eq(val, value[i]);
	}

	// cleanup
	for (i = 0; i < NUM_ENVVAR; i++) {
		mm_unsetenv(name[i]);
		ck_assert(mm_getenv(name[i], NULL) == NULL);
	}
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                       Path component parsing tests                     *
 *                                                                        *
 **************************************************************************/
static const struct {
	const char* path;
	const char* dir;
	const char* base;
} dir_comp_cases[] = {
	{.path = "/usr/lib",      .dir = "/usr",       .base = "lib"},
	{.path = "/usr/",         .dir = "/",          .base = "usr"},
	{.path = "usr",           .dir = ".",          .base = "usr"},
	{.path = "/",             .dir = "/",          .base = "/"},
	{.path = ".",             .dir = ".",          .base = "."},
	{.path = "..",            .dir = ".",          .base = ".."},
	{.path = "/usr//lib",     .dir = "/usr",       .base = "lib"},
	{.path = "/usr//lib//",   .dir = "/usr",       .base = "lib"},
	{.path = "/usr///",       .dir = "/",          .base = "usr"},
	{.path = "///usr/",       .dir = "/",          .base = "usr"},
	{.path = "///",           .dir = "/",          .base = "/"},
	{.path = "./",            .dir = ".",          .base = "."},
	{.path = "../",           .dir = ".",          .base = ".."},
	{.path = "",              .dir = ".",          .base = "."},
	{.path = "//",            .dir = "/",          .base = "/"},
	{.path = "...",           .dir = ".",          .base = "..."},
	{.path = " ",             .dir = ".",          .base = " "},
#if defined(_WIN32)
	{.path = "\\usr\\",       .dir = "\\",         .base = "usr"},
	{.path = "\\usr\\lib",    .dir = "\\usr",      .base = "lib"},
	{.path = "\\usr/lib",     .dir = "\\usr",      .base = "lib"},
	{.path = "\\usr/lib\\hi", .dir = "\\usr/lib",  .base = "hi"},
	{.path = "\\usr\\lib/hi", .dir = "\\usr\\lib", .base = "hi"},
#else
	{.path = "/1\\2/3", .dir = "/1\\2", .base = "3"},
#endif
};


START_TEST(parse_dirname)
{
	int i, explen;
	char result[64], path[64];
	const char* expected;

	for (i = 0; i < MM_NELEM(dir_comp_cases); i++) {
		strcpy(path, dir_comp_cases[i].path);
		expected = dir_comp_cases[i].dir;
		explen = strlen(expected);

		ck_assert(mm_dirname(NULL, path) == explen);

		// Run on different string buffer
		ck_assert(mm_dirname(result, path) == explen);
		ck_assert_str_eq(result, expected);
		ck_assert_str_eq(path, dir_comp_cases[i].path);

		// Run on same string buffer
		ck_assert(mm_dirname(path, path) == explen);
		ck_assert_str_eq(path, expected);
	}

	ck_assert(mm_dirname(NULL, NULL) == 1);
	ck_assert(mm_dirname(result, NULL) == 1);
	ck_assert_str_eq(result, ".");
}
END_TEST


START_TEST(parse_basename)
{
	int i, explen;
	char result[64], path[64];
	const char* expected;

	for (i = 0; i < MM_NELEM(dir_comp_cases); i++) {
		strcpy(path, dir_comp_cases[i].path);
		expected = dir_comp_cases[i].base;
		explen = strlen(expected);

		ck_assert(mm_basename(NULL, path) == explen);

		// Run on different string buffer
		ck_assert(mm_basename(result, path) == explen);
		ck_assert_str_eq(result, expected);
		ck_assert_str_eq(path, dir_comp_cases[i].path);

		// Run on same string buffer
		ck_assert(mm_basename(path, path) == explen);
		ck_assert_str_eq(path, expected);
	}

	ck_assert(mm_basename(NULL, NULL) == 1);
	ck_assert(mm_basename(result, NULL) == 1);
	ck_assert_str_eq(result, ".");
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_utils_tcase(void)
{
	TCase *tc = tcase_create("utils");

	tcase_add_loop_test(tc, get_basedir, -5, MM_NUM_DIRTYPE+5);
	tcase_add_loop_test(tc, path_from_base, -5, MM_NUM_DIRTYPE+5);
	tcase_add_test(tc, mmstrcasecmp_test);
	tcase_add_test(tc, get_set_unset_env);
	tcase_add_test(tc, get_environ);
	tcase_add_test(tc, multiple_set_unset_env);
	tcase_add_test(tc, append_prepend_environ);
	tcase_add_test(tc, parse_dirname);
	tcase_add_test(tc, parse_basename);

	return tc;
}


