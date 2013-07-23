/*
    Copyright (C) 2012-2013  MindMaze SA
    All right reserved

    Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
            Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmtype.h>
#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#define IMG_WIDTH 640
#define IMG_HEIGHT 480

START_TEST(bsize_1u_test)
{
	mmimage img =  {
		.width = 9,
		.height = 1,
		.depth = MM_DEPTH_1U,
		.nch = 1,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == 2);
}
END_TEST

START_TEST(bsize_8u_test)
{
	mmimage img =  {
		.width = IMG_WIDTH,
		.height = IMG_HEIGHT,
		.depth = MM_DEPTH_8U,
		.nch = 1,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == IMG_WIDTH * IMG_HEIGHT);
}
END_TEST

START_TEST(bsize_8uc3_test)
{
	mmimage img =  {
		.width = IMG_WIDTH,
		.height = IMG_HEIGHT,
		.depth = MM_DEPTH_8U,
		.nch = 3,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == IMG_WIDTH * IMG_HEIGHT * 3);
}
END_TEST

START_TEST(bsize_16u_test)
{
	mmimage img =  {
		.width = IMG_WIDTH,
		.height = IMG_HEIGHT,
		.depth = MM_DEPTH_16U,
		.nch = 1,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == IMG_WIDTH * IMG_HEIGHT * 2);
}
END_TEST

START_TEST(bsize_32f_test)
{
	mmimage img =  {
		.width = IMG_WIDTH,
		.height = IMG_HEIGHT,
		.depth = MM_DEPTH_32F,
		.nch = 1,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == IMG_WIDTH * IMG_HEIGHT * 4);
}
END_TEST

START_TEST(bsize_8s_test)
{
	mmimage img =  {
		.width = IMG_WIDTH,
		.height = IMG_HEIGHT,
		.depth = MM_DEPTH_8S,
		.nch = 1,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == IMG_WIDTH * IMG_HEIGHT);
}
END_TEST

START_TEST(bsize_16s_test)
{
	mmimage img =  {
		.width = IMG_WIDTH,
		.height = IMG_HEIGHT,
		.depth = MM_DEPTH_16S,
		.nch = 1,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == IMG_WIDTH * IMG_HEIGHT * 2);
}
END_TEST

START_TEST(bsize_32s_test)
{
	mmimage img =  {
		.width = IMG_WIDTH,
		.height = IMG_HEIGHT,
		.depth = MM_DEPTH_32S,
		.nch = 1,
		.data = NULL
	};
	ck_assert(mmimage_buffer_size(&img) == IMG_WIDTH * IMG_HEIGHT * 4);
}
END_TEST

static
Suite* type_suite(void)
{
	Suite *s = suite_create("Type");

	/* Core test case */
	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, bsize_1u_test);
	tcase_add_test(tc_core, bsize_8u_test);
	tcase_add_test(tc_core, bsize_8uc3_test);
	tcase_add_test(tc_core, bsize_16u_test);
	tcase_add_test(tc_core, bsize_32f_test);
	tcase_add_test(tc_core, bsize_8s_test);
	tcase_add_test(tc_core, bsize_16s_test);
	tcase_add_test(tc_core, bsize_32s_test);
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

