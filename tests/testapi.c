/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdlib.h>

#include "mmlib.h"
#include "mmsysio.h"
#include "api-testcases.h"

#define TEST_LOCK_REFEREE_SERVER_BIN    TOP_BUILDDIR"/src/"LT_OBJDIR"/lock-referee.exe"

static
Suite* api_suite(void)
{
	Suite *s = suite_create("API");

	suite_add_tcase(s, create_allocation_tcase());
	suite_add_tcase(s, create_time_tcase());
	suite_add_tcase(s, create_thread_tcase());
	suite_add_tcase(s, create_file_tcase());
	suite_add_tcase(s, create_socket_tcase());
	suite_add_tcase(s, create_ipc_tcase());
	suite_add_tcase(s, create_dir_tcase());
	suite_add_tcase(s, create_shm_tcase());
	suite_add_tcase(s, create_dlfcn_tcase());
	suite_add_tcase(s, create_process_tcase());
	suite_add_tcase(s, create_argparse_tcase());
	suite_add_tcase(s, create_utils_tcase());

	return s;
}


int main(void)
{
	Suite* suite;
	SRunner* runner;
	int exitcode = EXIT_SUCCESS;

	mm_chdir(BUILDDIR);

#if defined(_WIN32)
	mm_setenv("MMLIB_LOCKREF_BIN", TEST_LOCK_REFEREE_SERVER_BIN, MM_ENV_OVERWRITE);
	mm_setenv("PATH", TOP_BUILDDIR"/src/"LT_OBJDIR, MM_ENV_PREPEND);
#endif

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


