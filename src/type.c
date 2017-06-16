/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <mmerrno.h>
#include <string.h>

#include "mmlog.h"
#include "mmtype.h"

#define CACHE_LINE_SIZE	64

/**
 * mmimg_pixel_size() - get the size in byte a pixel in a specified format
 * @pixfmt:     pixel format
 *
 * Return: the size in bytes of a pixel, 0 with errno set in case of
 * unknown pixel format.
 */
API_EXPORTED
size_t mmimg_pixel_size(unsigned int pixfmt)
{
	size_t pixsize;

	// Get pixel size of one component of a pixel
	switch (pixfmt & MM_PIXFMT_DATATYPE_MASK) {
	case MM_PIXFMT_UINT8: pixsize = 1; break;
	case MM_PIXFMT_UINT16: pixsize = 2; break;
	case MM_PIXFMT_UINT32: pixsize = 4; break;
	case MM_PIXFMT_FLOAT: pixsize = sizeof(float); break;
	case MM_PIXFMT_DOUBLE: pixsize = sizeof(double); break;
	default: goto error;
	}

	// Multiply component size by the computed number of component
	switch (pixfmt & MM_PIXFMT_COMP_MASK) {
	case MM_PIXFMT_COMP_MONO:
		return pixsize;

	case MM_PIXFMT_COMP_RGB:
	case MM_PIXFMT_COMP_BGR:
	case MM_PIXFMT_COMP_HSV:
	case MM_PIXFMT_COMP_HLS:
		return 3*pixsize;

	case MM_PIXFMT_COMP_RGBA:
	case MM_PIXFMT_COMP_BGRA:
	case MM_PIXFMT_COMP_HSVA:
	case MM_PIXFMT_COMP_HLSA:
		return 4*pixsize;
	};

error:
	mm_raise_error(EINVAL, "Unknown pixel format: %08x", pixfmt);
	return 0;
}


/**
 * mmimg_set_stride() - Compute a stride suitable for an alignment
 * @img:        Image description whose stride must be computed
 * @alignment:  Alignment to use for stride (may be 0)
 *
 * This function will modify the image description for having a stride
 * compatible with the specified alignment. If this alignment is 0, the
 * function will choose an good alignment for you.
 *
 * Return: 0 in case of sucess, otherwise -1 with errno set appropriately.
 */
API_EXPORTED
int mmimg_set_stride(struct mm_imgdesc* img, size_t alignment)
{
	unsigned int stride, remainder;

	if (!img) {
		mm_raise_error(EINVAL, "Missing img argument");
		return -1;
	}

	// Compute a stride assuming unpadded data lines
	stride = img->width * mmimg_pixel_size(img->pixformat);
	if (!stride)
		return -1;

	// Round up the next multiple of alignment
	if (!alignment)
		alignment = CACHE_LINE_SIZE;
	remainder = stride % alignment;
	if (remainder)
		stride += alignment - remainder;

	img->stride = stride;
	return 0;
}


/**
 * mmimg_alloc_buffer() - Allocate an image buffer with suitable alignment
 * @img:        Image description
 *
 * This function allocate a buffer for an image specified by the description
 * provided in argument. The allocated buffer will have an optimal alignment
 * suitable for the CPU.
 *
 * To deallocate the returned buffer, use free() of the C standard library.
 *
 * Return: the pointer to the allocated buffer in case of success, NULL
 * otherwise with errno set appropriately.
 */
API_EXPORTED
void* mmimg_alloc_buffer(const struct mm_imgdesc* img)
{
	void* ptr;
	size_t bsize;
	int ret;

	if (!img) {
		mm_raise_error(EINVAL, "Missing img argument");
		return NULL;
	}

	bsize = img->height*img->stride;

	// TODO: calculate alignment suitable for CPU at runtime
	ret = posix_memalign(&ptr, CACHE_LINE_SIZE, bsize);
	if (ret) {
		mm_raise_error(ret, "Cannot allocate img buffer: %s", strerror(ret));
		return NULL;
	}

	return ptr;
}


API_EXPORTED
size_t mmimage_buffer_size(const mmimage* img)
{
	return ( img->width * img->height * img->nch
		* ( img->depth & ~MM_DEPTH_SIGN ) + 7 ) / 8;
}
