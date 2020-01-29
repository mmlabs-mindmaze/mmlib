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
#include "mmlog.h"
#include "mmerrno.h"
#include "mmpredefs.h"

#define NUM_ALLOC	30

static int prev_max_loglvl;
static void setup(void)
{
	prev_max_loglvl = mm_log_set_maxlvl(MM_LOG_NONE);
}

static void teardown(void)
{
	mm_log_set_maxlvl(prev_max_loglvl);
}

START_TEST(aligned_heap_allocation)
{
	void* ptr;
	size_t align, size;
	int i;

	for (i = 1; i < NUM_ALLOC; i++) {
		for (align = sizeof(void*); align <= MM_PAGESZ; align *= 2) {
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


START_TEST(aligned_heap_allocation_error)
{
#if !defined(__SANITIZE_ADDRESS__)
	void* ptr;
	size_t align;
	struct mm_error_state errstate;

	mm_save_errorstate(&errstate);

	// Test error if alignment is not power of 2 of pointer size
	for (align = 0; align < MM_PAGESZ; align++) {
		if (align >= sizeof(void*) && MM_IS_POW2(align))
			continue;

		ptr = mm_aligned_alloc(align, 4*MM_PAGESZ);
		ck_assert(ptr == NULL);
		ck_assert(mm_get_lasterror_number() == EINVAL);
	}

	// Test error is size is too big
	ptr = mm_aligned_alloc(sizeof(void*), SIZE_MAX);
	ck_assert(ptr == NULL);
	ck_assert(mm_get_lasterror_number() == ENOMEM);

	mm_set_errorstate(&errstate);
#endif /* !__SANITIZE_ADDRESS__ */
}
END_TEST


static size_t stack_alloc_sizes[] = {
	1, 3, sizeof(double), 64, 57, 256, 950, 2044, 2048, 2056, 4032,
};

START_TEST(aligned_stack_allocation)
{
	void* ptr;
	size_t align, size;

	size = stack_alloc_sizes[_i];

	for (align = sizeof(void*); align <= 2048; align *= 2) {
		ptr = mm_aligned_alloca(align, size);
		ck_assert(ptr != NULL);
		ck_assert_int_eq((uintptr_t)ptr & (align-1), 0);
		memset(ptr, 'x', size);
	}
}
END_TEST


static size_t malloca_sizes[] = {
	1, 3, sizeof(double), 64, 57, 256, 950, 2044, 2048, 2056, 4032,
	4096, 6*4091, 6*4096, 10000000, 100000000
};

START_TEST(safe_stack_allocation)
{
	void* ptr;
	size_t size = malloca_sizes[_i];

	ptr = mm_malloca(size);
	ck_assert(ptr != NULL);
	memset(ptr, 'x', size);

	ck_assert_int_eq((uintptr_t)ptr & (sizeof(char)-1), 0);
	ck_assert_int_eq((uintptr_t)ptr & (sizeof(short)-1), 0);
	ck_assert_int_eq((uintptr_t)ptr & (sizeof(int)-1), 0);
	ck_assert_int_eq((uintptr_t)ptr & (sizeof(long)-1), 0);
	ck_assert_int_eq((uintptr_t)ptr & (sizeof(float)-1), 0);
	ck_assert_int_eq((uintptr_t)ptr & (sizeof(double)-1), 0);

	mm_freea(ptr);
}
END_TEST


START_TEST(safe_stack_allocation_error)
{
#if !defined(__SANITIZE_ADDRESS__)
	void* ptr;
	size_t rem_sz;
	struct mm_error_state errstate;

	mm_save_errorstate(&errstate);

	for (rem_sz = 0; rem_sz < 4*MM_STK_ALIGN; rem_sz++) {
		ptr = mm_malloca(SIZE_MAX - rem_sz);
		ck_assert(ptr == NULL);
		ck_assert(mm_get_lasterror_number() == ENOMEM);
	}

	mm_set_errorstate(&errstate);
#endif /* !__SANITIZE_ADDRESS__ */
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
	tcase_add_checked_fixture(tc, setup, teardown);

	tcase_add_test(tc, aligned_heap_allocation);
	tcase_add_test(tc, aligned_heap_allocation_error);
	tcase_add_loop_test(tc, aligned_stack_allocation,
	                    0, MM_NELEM(stack_alloc_sizes));
	tcase_add_loop_test(tc, safe_stack_allocation,
	                    0, MM_NELEM(malloca_sizes));
	tcase_add_test(tc, safe_stack_allocation_error);

	return tc;
}


