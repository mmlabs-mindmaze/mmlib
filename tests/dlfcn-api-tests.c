/*
   @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mmdlfcn.h>
#include <mmerrno.h>

#include "api-testcases.h"
#include "dynlib-api.h"

#define TEST_VAL 0x1f2f3f4f


START_TEST(dlopen_simple)
{
	mm_dynlib_t * hndl = mm_dlopen(LT_OBJDIR "/dynlib-test" LT_MODULE_EXT,
	                              MM_LD_NOW);
	ck_assert(hndl != NULL);
	mm_dlclose(hndl);
}
END_TEST

START_TEST(dlopen_invalid_flags)
{
	mm_dynlib_t * hndl = mm_dlopen(LT_OBJDIR "/dynlib-test" LT_MODULE_EXT,
	                              MM_LD_NOW | MM_LD_LAZY);
	ck_assert(hndl == NULL);
	ck_assert(mm_get_lasterror_number() == EINVAL);
	mm_dlclose(hndl);
}
END_TEST

START_TEST(dlopen_invalid_path)
{
	mm_dynlib_t * hndl = mm_dlopen("invalid-path-name",
	                              MM_LD_NOW | MM_LD_LAZY);
	ck_assert(hndl == NULL);
	ck_assert(mm_get_lasterror_number() == EINVAL);
	mm_dlclose(hndl);
}
END_TEST

START_TEST(dlsym_simple)
{
	mm_dynlib_t * hndl = mm_dlopen(LT_OBJDIR "/dynlib-test" LT_MODULE_EXT,
	                              MM_LD_NOW);
	ck_assert(hndl != NULL);
	ck_assert(mm_dlsym(hndl, "api") != NULL);
	mm_dlclose(hndl);
}
END_TEST

START_TEST(dlsym_invalid)
{
	ck_assert(mm_dlsym(NULL, "invalid-symbol-name") == NULL);
	ck_assert(mm_get_lasterror_number() == EINVAL);


	mm_dynlib_t * hndl = mm_dlopen(LT_OBJDIR "/dynlib-test" LT_MODULE_EXT,
	                              MM_LD_NOW);
	ck_assert(hndl != NULL);

	ck_assert(mm_dlsym(hndl, NULL) == NULL);
	ck_assert(mm_get_lasterror_number() == EINVAL);

	mm_dlclose(hndl);
}
END_TEST

START_TEST(dlsym_not_found)
{
	mm_dynlib_t * hndl = mm_dlopen(LT_OBJDIR "/dynlib-test" LT_MODULE_EXT,
	                              MM_LD_NOW);
	ck_assert(hndl != NULL);
	ck_assert(mm_dlsym(hndl, "invalid-symbol-name") == NULL);
	mm_dlclose(hndl);
}
END_TEST

START_TEST(dl_fileext_test)
{
#ifdef LT_MODULE_EXT
	ck_assert(strncmp(mm_dl_fileext(), LT_MODULE_EXT,
	                  sizeof(LT_MODULE_EXT)) == 0);
#endif
	return;
}
END_TEST

static
bool check_dynlibdata_val(struct dynlib_data* data,
                          int expected_ival, const char* expected_str)
{
	if ((data->intval != expected_ival)
	   || (strcmp(data->str, expected_str) != 0) )
		return false;

	return true;
}

static
int run_plugin_tests(const char* plugin, int flags)
{
	mm_dynlib_t* libhnd;
	struct dynlib_data* data;
	struct dynlib_vtab* vtab;

	libhnd = mm_dlopen(plugin, flags);
	if (!libhnd)
		goto error;

	// Test symbol loading
	if (!(data = mm_dlsym(libhnd, "libdata"))
	   || !(vtab = mm_dlsym(libhnd, "api")) )
		goto error;

	// Test initial values
	if (!check_dynlibdata_val(data, INITIAL_INTVAL, INITIAL_STR)
	   || (vtab->read_internal_code() != 0) ) {
		mm_raise_error(MM_EWRONGSTATE, "Wrong initial values");
		goto error;
	}

	vtab->set_internal_code(TEST_VAL);
	if (vtab->read_internal_code() != TEST_VAL) {
		mm_raise_error(MM_EWRONGSTATE, "copied values do not match");
		goto error;
	}

	vtab->set_data(-2, "test");
	if (!check_dynlibdata_val(data, -2, "test")) {
		mm_raise_error(MM_EWRONGSTATE, "values not set in libdata");
		goto error;
	}

	vtab->reset_data();
	if (!check_dynlibdata_val(data, INITIAL_INTVAL, INITIAL_STR) ) {
		mm_raise_error(MM_EWRONGSTATE, "Wrong value reset");
		goto error;
	}

	mm_dlclose(libhnd);
	return 0;

error:
	fprintf(stderr, "run_plugin_tests(\"%s\", %08x) failed: %s",
	        plugin, flags, mm_get_lasterror_desc());
	if (libhnd)
		mm_dlclose(libhnd);

	return -1;
}


START_TEST(plugin_lt_module_ext)
{
	ck_assert(run_plugin_tests(LT_OBJDIR "/dynlib-test" LT_MODULE_EXT, 0) ==
	          0);
}
END_TEST

START_TEST(plugin_ld_append_ext)
{
	ck_assert(run_plugin_tests(LT_OBJDIR "/dynlib-test", MM_LD_APPEND_EXT) ==
	          0);
}
END_TEST

LOCAL_SYMBOL
TCase* create_dlfcn_tcase(void)
{
	TCase * tc;

	tc = tcase_create("dlfcn");

	tcase_add_test(tc, dlopen_simple);
	tcase_add_test(tc, dlopen_invalid_flags);
	tcase_add_test(tc, dlopen_invalid_path);
	tcase_add_test(tc, dlsym_simple);
	tcase_add_test(tc, dlsym_invalid);
	tcase_add_test(tc, dlsym_not_found);
	tcase_add_test(tc, dl_fileext_test);

	tcase_add_test(tc, plugin_lt_module_ext);
	tcase_add_test(tc, plugin_ld_append_ext);

	return tc;
}
