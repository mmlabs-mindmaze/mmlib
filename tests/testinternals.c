/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "internals-testcases.h"

static
Suite* internal_suite(void)
{
	Suite *s = suite_create("Internals");

	suite_add_tcase(s, create_case_log_internals());

	return s;
}


int main(void)
{
	Suite* suite;
	SRunner* runner;
	int exitcode = EXIT_SUCCESS;

	suite = internal_suite();
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

