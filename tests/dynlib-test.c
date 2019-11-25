/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "dynlib-api.h"

#include <string.h>

#define DYNLIB_DATA_INITIALIZER	{.intval = INITIAL_INTVAL, .str = INITIAL_STR}

static int internal_code;

API_EXPORTED_RELOCATABLE
struct dynlib_data libdata = DYNLIB_DATA_INITIALIZER;



static
void dynlib_set_data(int ival, const char* str)
{
	libdata.intval = ival;
	strncpy(libdata.str, str, sizeof(libdata.str)-1);
}


static
void dynlib_reset_data(void)
{
	libdata = (struct dynlib_data)DYNLIB_DATA_INITIALIZER;
}


static
int dynlib_read_internal_code(void)
{
	return internal_code;
}


static
void dynlib_set_internal_code(int val)
{
	internal_code = val;
}


API_EXPORTED
struct dynlib_vtab api = {
	.set_data = dynlib_set_data,
	.reset_data = dynlib_reset_data,
	.read_internal_code = dynlib_read_internal_code,
	.set_internal_code = dynlib_set_internal_code,
};


