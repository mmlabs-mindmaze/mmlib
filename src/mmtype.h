/*
   @mindmaze_header@
*/
#ifndef MMTYPE_H
#define MMTYPE_H

#include <stddef.h>

/* Image depth definitions */
#define MM_DEPTH_SIGN 0x80000000
#define MM_DEPTH_1U     1
#define MM_DEPTH_8U     8
#define MM_DEPTH_16U   16
#define MM_DEPTH_32F   32
#define MM_DEPTH_8S  (MM_DEPTH_SIGN|MM_DEPTH_8U)
#define MM_DEPTH_16S (MM_DEPTH_SIGN|MM_DEPTH_16U)
#define MM_DEPTH_32S (MM_DEPTH_SIGN|32)

/* Joint indexes */
#define MM_HEAD 0
#define MM_NECK 1
#define MM_L_SHOULDER 2
#define MM_R_SHOULDER 3
#define MM_L_ELBOW 4
#define MM_R_ELBOW 5
#define MM_L_HAND 6
#define MM_R_HAND 7
#define MM_TORSO 8

typedef struct mmimage {
	int nch;     /* Most functions support 1,2,3 or 4 channels */
	int depth;   /* Pixel depth in bits: U for unsigned, S for
	                signed and F for floating point */
	int width;   /* Image width in pixels. */
	int height;  /* Image height in pixels. */
	void* data;  /* Pointer to image data. */
} mmimage;

typedef struct epos3d {
	float v[3];
	int confidence;	  /* Confidence in the estimation */
} epos3d;

typedef struct rotmatrix3d {
	float elem[9];
	int confidence;
} rotmatrix3d;

typedef struct mmquat {
	float v[4];
	int confidence;
} mmquat;

#ifdef __cplusplus
extern "C" {
#endif

size_t mmimage_buffer_size(const mmimage* img);

#ifdef __cplusplus
}
#endif

#endif /* MMTYPE_H */
