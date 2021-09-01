/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdlib.h>
#include <stdio.h>

#include "mmlib.h"
#include "mmsysio.h"
#include "api-testcases.h"

#define TEST_LOCK_REFEREE_SERVER_BIN    TOP_BUILDDIR"/src/"LT_OBJDIR"/lock-referee.exe"

static
void flush_stdout(void)
{
	fflush(stdout);
}


static
Suite* api_suite(void)
{
	Suite *s;
	int i;
	TCase* tc[] = {
		create_allocation_tcase(),
		create_time_tcase(),
		create_thread_tcase(),
		create_file_tcase(),
		create_socket_tcase(),
		create_ipc_tcase(),
		create_dir_tcase(),
		create_shm_tcase(),
		create_dlfcn_tcase(),
		create_process_tcase(),
		create_argparse_tcase(),
		create_utils_tcase(),
		create_advanced_file_tcase(),
	};

	s = suite_create("API");

	for (i = 0; i < MM_NELEM(tc); i++) {
		tcase_add_checked_fixture(tc[i], flush_stdout, flush_stdout);
		suite_add_tcase(s, tc[i]);
	}

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


