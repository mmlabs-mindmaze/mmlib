/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdlib.h>
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
	int number_failed;
	Suite *s = internal_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_ENV);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

