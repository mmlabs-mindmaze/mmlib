/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "../src/startup-win32.c"

#include <check.h>

#include "internals-testcases.h"

struct cmdline_case {
	const char* cmdline;
	int argc;
	char* argv[16];
};

struct cmdline_case cmdline_cases[] = {
	{.cmdline = "prog", .argc = 1,
	 .argv = {"prog"}},
	{.cmdline = "\"prog space\"", .argc = 1,
	 .argv = {"prog space"}},
	{.cmdline = "prog \"arg\"", .argc = 2,
	 .argv = {"prog", "arg"}},
	{.cmdline = "prog \\\"arg\\\"", .argc = 2,
	 .argv = {"prog", "\"arg\""}},
	{.cmdline = "prog \\\\\"arg1 arg2\\\\\"", .argc = 2,
	 .argv = {"prog", "\\arg1 arg2\\"}},
	{.cmdline = "prog \\\"arg arg2\\\"", .argc = 3,
	 .argv = {"prog", "\"arg", "arg2\""}},
	{.cmdline = "prog arg1 arg2", .argc = 3,
	 .argv = {"prog", "arg1", "arg2"}},
	{.cmdline = "prog \"arg1 arg2\"remaining", .argc = 2,
	 .argv = {"prog", "arg1 arg2remaining"}},
};

START_TEST(split_cmdline)
{
	int i, argc;
	const char* cmdline = cmdline_cases[_i].cmdline;
	char** ref_argv = cmdline_cases[_i].argv;
	int ref_argc = cmdline_cases[_i].argc;
	char** argv;

	argc = parse_cmdline(&argv, cmdline);
	ck_assert_int_eq(argc, ref_argc);

	for (i = 0; i < argc; i++)
		ck_assert_str_eq(argv[i], ref_argv[i]);

	free(argv);
}
END_TEST


LOCAL_SYMBOL
TCase* create_case_startup_win32_internals(void)
{
	TCase *tc = tcase_create("startup-win32_internals");
	tcase_add_loop_test(tc, split_cmdline, 0, MM_NELEM(cmdline_cases));

	return tc;
}


