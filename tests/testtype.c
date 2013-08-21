/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmtype.h>
#include <stdio.h>
#include <stdlib.h>
#include <check.h>

struct img_size {
	int w, h;
	int d;
	int nch;
	size_t sz;
};

static const
struct img_size exp_imgsz[] = {
	{.w = 9, .h = 1, .d = MM_DEPTH_1U, .nch = 1, .sz = 2},
	{.w = 640, .h = 480, .d = MM_DEPTH_8U, .nch = 1, .sz = 640*480},
	{.w = 640, .h = 480, .d = MM_DEPTH_8U, .nch = 3, .sz = 640*480*3},
	{.w = 640, .h = 480, .d = MM_DEPTH_16U, .nch = 1, .sz = 640*480*2},
	{.w = 640, .h = 480, .d = MM_DEPTH_32F, .nch = 1, .sz = 640*480*4},
	{.w = 640, .h = 480, .d = MM_DEPTH_8S, .nch = 1, .sz = 640*480},
	{.w = 640, .h = 480, .d = MM_DEPTH_16S, .nch = 1, .sz = 640*480*2},
	{.w = 640, .h = 480, .d = MM_DEPTH_32S, .nch = 1, .sz = 640*480*4},
};
#define NUM_IMG_SIZE	(sizeof(exp_imgsz)/sizeof(exp_imgsz[0]))

START_TEST(buffer_size_test)
{
	mmimage img =  {
		.width = exp_imgsz[_i].w,
		.height = exp_imgsz[_i].h,
		.depth = exp_imgsz[_i].d,
		.nch = exp_imgsz[_i].nch,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == exp_imgsz[_i].sz);
}
END_TEST

static
Suite* type_suite(void)
{
	Suite *s = suite_create("Type");

	/* Core test case */
	TCase *tc_core = tcase_create("Core");
	tcase_add_loop_test(tc_core, buffer_size_test, 0, NUM_IMG_SIZE);
	suite_add_tcase(s, tc_core);

	return s;
}


int main(void)
{
	int number_failed;
	Suite *s = type_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_ENV);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

