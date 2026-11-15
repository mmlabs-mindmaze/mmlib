/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>

#include "utils-win32.h"
#include "internals-testcases.h"


START_TEST(read_thread_id)
{
	ck_assert(get_tid() == GetCurrentThreadId());
}
END_TEST


START_TEST(read_tls_value)
{
	DWORD tls_index;
	void* ptr = (void*)0x11223344;

	tls_index = TlsAlloc();
	TlsSetValue(tls_index, ptr);

	ck_assert_ptr_eq(tls_get_value(tls_index), TlsGetValue(tls_index));
}
END_TEST


LOCAL_SYMBOL
TCase* create_case_threadlocal_win32_internals(void)
{
	TCase *tc = tcase_create("threadlocal-win32-internals");

	tcase_add_test(tc, read_thread_id);
	tcase_add_test(tc, read_tls_value);

	return tc;
}
