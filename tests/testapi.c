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


	return s;
}


int main(void)
{
	int number_failed;
	Suite *s = api_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_ENV);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


