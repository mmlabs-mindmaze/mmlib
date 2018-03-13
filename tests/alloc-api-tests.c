/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdint.h>

#include "api-testcases.h"
#include "mmlib.h"

#define NUM_ALLOC	30

START_TEST(aligned_heap_allocation)
{
	void* ptr;
	size_t align, size;
	int i;

	for (i = 1; i < NUM_ALLOC; i++) {
		for (align = sizeof(void*); align <= 4096; align *= 2) {
			size = i * align;
			ptr = mm_aligned_alloc(align, size);
			ck_assert(ptr != NULL);
			ck_assert_int_eq((uintptr_t)ptr & (align-1), 0);
			memset(ptr, 'x', size);
			mm_aligned_free(ptr);
		}
	}
}
END_TEST

/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_allocation_tcase(void)
{
	TCase *tc = tcase_create("allocation");

	tcase_add_test(tc, aligned_heap_allocation);

	return tc;
}


