/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <mmlog.h>
#include <setjmp.h>
#include <signal.h>

static
void logged_func(void)
{
	mmlog_warn("logged_func");
}

#undef MMLOG_MODULE_NAME
#define MMLOG_MODULE_NAME "testlog"

// Stolen from here:
// https://stackoverflow.com/questions/8934879/how-to-handle-sigabrt-signal
jmp_buf env;

static
void on_sigabrt (int signum)
{
	(void)signum;
	longjmp (env, 1);
}

// Returns 1 iff the function does not abort
static
int try_and_catch_abort (void (*func)(void))
{
	if (setjmp (env) == 0) {
		signal(SIGABRT, &on_sigabrt);
		(*func)();
		return 1;
	} else {
		return 0;
	}
}		

static
void provoke_a_crash(void)
{
	mm_crash("We should crash");
}

static
void do_not_provoke_a_crash(void)
{
	mmlog_info("This should not crash");
}

static
int test_crash(void)
{
	return !try_and_catch_abort(&provoke_a_crash) 
		&& try_and_catch_abort(&do_not_provoke_a_crash);
}

static
void provoke_a_check_failure(void)
{
	mm_check(119 == 7*16, "Actually, it should be 7*%d", 119/7);
}

static
void do_not_provoke_a_check_failure(void)
{
	mm_check(119 == 7*17);
}

static
int test_check(void)
{
	return !try_and_catch_abort(&provoke_a_check_failure)
		&& try_and_catch_abort(&do_not_provoke_a_check_failure);
}

static
int test_basic_logging(void)
{
	int i;

	mmlog_info("Starting logging...");

	for (i = -5; i<5; i++) {
		logged_func();
		if (i != 0)
			mmlog_warn("Everything is fine (%i)", i);
		else
			mmlog_error("Null value (i == %i)", i);
	}

	mmlog_info("Stop logging.");
	return 1;
}

int main(void)
{
	return (test_basic_logging()
					&& test_crash() 
					&& test_check())? 
		EXIT_SUCCESS : EXIT_FAILURE;
}
