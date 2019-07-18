/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

/* disable error logging for this test module */
#define MMLOG_MAXLEVEL 0

#include <mmtype.h>
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "api-testcases.h"
#include "mmlog.h"

#define NUMEL(array) (sizeof(array)/sizeof(array[0]))

static int prev_max_loglvl;
static void setup(void)
{
	prev_max_loglvl = mmlog_set_maxlvl(MMLOG_NONE);
}

static void teardown(void)
{
	mmlog_set_maxlvl(prev_max_loglvl);
}

/**************************************************************************
 *                                                                        *
 *                          struct mmimage tests                          *
 *                                                                        *
 **************************************************************************/
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


/**************************************************************************
 *                                                                        *
 *                         struct mm_imgdesc tests                        *
 *                                                                        *
 **************************************************************************/
#define MAX_WIDTH	160
#define MAX_ALIGNMENT	32

struct pixsize {
	unsigned int fmt;
	size_t sz;
};

static const
struct pixsize exp_pixsz[] = {
	{.fmt = MM_PIXFMT_MONO8, .sz = 1},
	{.fmt = MM_PIXFMT_MONO16, .sz = 2},
	{.fmt = MM_PIXFMT_BGRA, .sz = 4},
	{.fmt = MM_PIXFMT_RGBA, .sz = 4},
	{.fmt = MM_PIXFMT_BGR, .sz = 3},
	{.fmt = MM_PIXFMT_RGB, .sz = 3},
	{.fmt = 0xFFFFFFFF, .sz = 0},
};


START_TEST(pixel_size_test)
{
	unsigned int pixel_format = exp_pixsz[_i].fmt;
	size_t pixel_size = exp_pixsz[_i].sz;
	ck_assert(mmimg_pixel_size(pixel_format) == pixel_size);
}
END_TEST


START_TEST(valid_stride_test)
{
	unsigned int ifmt;
	size_t pixel_size, alignment = _i;
	struct mm_imgdesc img = { .height = 120 };

	for (ifmt = 0; ifmt < NUMEL(exp_pixsz); ifmt++) {
		img.pixformat = exp_pixsz[ifmt].fmt;
		pixel_size = mmimg_pixel_size(img.pixformat);

		for (img.width = 1; img.width < MAX_WIDTH; img.width++) {
			mmimg_set_stride(&img, alignment);
			ck_assert(img.stride >= img.width*pixel_size);
			ck_assert(img.stride % alignment == 0);
		}
	}
}
END_TEST


START_TEST(alloc_imgbuf_test)
{
	void* buffer;
	struct mm_imgdesc img = {
		.width = 235,
		.height = 120,
		.pixformat = MM_PIXFMT_BGR,
	};

	mmimg_set_stride(&img, _i);
	buffer = mmimg_alloc_buffer(&img);
	ck_assert(buffer != NULL);

	// check the whole allocated buffer is writable
	memset(buffer, 0, img.height*img.stride);

	mmimg_free_buffer(buffer);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                             test suite setup                           *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_type_tcase(void)
{
	/* Core test case */
	TCase *tc = tcase_create("type");
	tcase_add_checked_fixture(tc, setup, teardown);

	tcase_add_loop_test(tc, buffer_size_test, 0, NUMEL(exp_imgsz));
	tcase_add_loop_test(tc, pixel_size_test, 0, NUMEL(exp_pixsz));
	tcase_add_loop_test(tc, valid_stride_test, 1, MAX_ALIGNMENT);
	tcase_add_loop_test(tc, alloc_imgbuf_test, 0, MAX_ALIGNMENT);

	return tc;
}
