/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "../src/log.c"

#include <check.h>
#include <stdarg.h>


// Not used in the test... Define to a dummy function just for the sake of a
// successful compilation
API_EXPORTED
ssize_t mm_write(int fd, const void* buf, size_t nbyte)
{
	(void)fd;
	(void)buf;
	(void)nbyte;

	return -1;
}


static
size_t format_string(char* buff, size_t buflen, const char* msg, ...)
{
	size_t r;
	va_list args;

	va_start(args, msg);
	r = format_log_str(buff, buflen, MMLOG_DEBUG, "here", msg, args);
	va_end(args);

	return r;
}


START_TEST(log_overflow)
{
	size_t len;
	char buff[MMLOG_LINE_MAXLEN + 32];
	char arg[MMLOG_LINE_MAXLEN + 32];
	
	// Fill a message that must overflow the log string
	memset(arg, 'a', sizeof(arg)-1);
	arg[sizeof(arg)-1] = '\0';

	// Format log string
	len = format_string(buff, MMLOG_LINE_MAXLEN, "hello %s", arg);

	ck_assert(len <= MMLOG_LINE_MAXLEN);
	ck_assert(buff[len-1] == '\n');
}
END_TEST


static
Suite* internal_suite(void)
{
	Suite *s = suite_create("Geometry");

	TCase *tc_core = tcase_create("log internals");
	tcase_add_test(tc_core, log_overflow);
	suite_add_tcase(s, tc_core);

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

