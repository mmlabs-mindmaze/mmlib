/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include "random-internal.h"
#include "mmpredefs.h"

#include "internals-testcases.h"

#define MAX_TEST_SZ     512
#define NUM_REPEAT	4


static size_t len_cases[] = {4, 8, 32, 256, MAX_TEST_SZ};


START_TEST(test_get_random)
{
	char buff[NUM_REPEAT][MAX_TEST_SZ];
	size_t len = len_cases[_i];
	int i, j;

	for (i = 0; i < NUM_REPEAT; i++)
		ck_assert(get_random(buff[i], len) == 0);

	for (i = 0; i < NUM_REPEAT; i++) {
		for (j = i+1; j < NUM_REPEAT; j++) {
			ck_assert(memcmp(buff[i], buff[j], len) != 0);
		}
	}
}
END_TEST


LOCAL_SYMBOL
TCase* create_case_random_internals(void)
{
	TCase *tc = tcase_create("random internals");
	tcase_add_loop_test(tc, test_get_random, 0, MM_NELEM(len_cases));

	return tc;
}

