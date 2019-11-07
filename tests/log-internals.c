/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "../src/log.c"

#include <check.h>
#include <stdarg.h>

#include "internals-testcases.h"


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


LOCAL_SYMBOL
TCase* create_case_log_internals(void)
{
	TCase *tc = tcase_create("log internals");
	tcase_add_test(tc, log_overflow);

	return tc;
}

