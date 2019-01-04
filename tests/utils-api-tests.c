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

	return tc;
}


