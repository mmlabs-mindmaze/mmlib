/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdlib.h>

#include "api-testcases.h"

static
Suite* api_suite(void)
{
	Suite *s = suite_create("API");

	suite_add_tcase(s, create_type_tcase());
	suite_add_tcase(s, create_geometry_tcase());

	return s;
}


int main(void)
{
	Suite* suite;
	SRunner* runner;
	int exitcode = EXIT_SUCCESS;

	suite = api_suite();
	runner = srunner_create(suite);

#ifdef CHECK_SUPPORT_TAP
	srunner_set_tap(runner, "-");
#endif

	srunner_run_all(runner, CK_ENV);

#ifndef CHECK_SUPPORT_TAP
	if (srunner_ntests_failed(runner) != 0)
		exitcode = EXIT_FAILURE;
#endif

	srunner_free(runner);
	return exitcode;
}


