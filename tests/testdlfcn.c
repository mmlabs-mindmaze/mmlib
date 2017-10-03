/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "dynlib-api.h"

#include <mmdlfcn.h>
#include <mmerrno.h>
#include <stdlib.h>
#include <stdbool.h>

#define TEST_VAL	0x1f2f3f4f

static
bool check_dynlibdata_val(struct dynlib_data* data,
                          int expected_ival, const char* expected_str)
{
	if ( (data->intval != expected_ival)
	  || (strcmp(data->str, expected_str) != 0) )
		return false;

	return true;
}

static
int run_plugin_tests(const char* plugin, int flags)
{
	mmdynlib_t* libhnd;
	struct dynlib_data* data;
	struct dynlib_vtab* vtab;

	libhnd = mm_dlopen(plugin, flags);
	if (!libhnd)
		goto error;

	// Test symbol loading
	if ( !(data = mm_dlsym(libhnd, "libdata"))
	  || !(vtab = mm_dlsym(libhnd, "api")) )
		goto error;

	// Test initial values
	if ( !check_dynlibdata_val(data, INITIAL_INTVAL, INITIAL_STR)
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
	if ( !check_dynlibdata_val(data, INITIAL_INTVAL, INITIAL_STR) )	{
		mm_raise_error(MM_EWRONGSTATE, "Wrong value reset");
		goto error;
	}

	mm_dlclose(libhnd);
	return 0;

error:
	mm_print_lasterror("run_plugin_tests(\"%s\", %08x) failed", plugin, flags);
	if (libhnd)
		mm_dlclose(libhnd);

	return -1;
}


int main(void)
{
	if ( run_plugin_tests(LT_OBJDIR "dynlib-test" LT_MODULE_EXT, 0)
	  || run_plugin_tests(LT_OBJDIR "dynlib-test", MMLD_APPEND_EXT) )
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
