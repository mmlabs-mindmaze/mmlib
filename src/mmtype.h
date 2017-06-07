/*
   @mindmaze_header@
*/
#ifndef MMTYPE_H
#define MMTYPE_H

#include <stddef.h>

struct mm_error_state {
	char data[1024];
};

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


/**
 * struct camera_calibration - Calibration parameters of a camera
 * @resolution:  resolution along each axis
 * @focal:       focal lengths expressed in pixel units
 * @principal:   principal point that is usually at the image center
 * @distradial:  radial distortion coefficients in the order [k1 k2 k3 k4 k5 k6]
 * @disttangent: radial distortion coefficients in the order [p1 p2]
 * @rotation:    rotation matrix of the camera in the reference frame
 * @translation: translation of the focal point in the reference frame
 *
 * See http://docs.opencv.org/modules/calib3d/doc/camera_calibration_and_3d_reconstruction.html
 */
struct camera_calibration {
	int   resolution[2];
	float focal[2];
	float principal[2];
	float distradial[6];
	float disttangent[2];
	float rotation[9];
	float translation[3];
};

/**
 * DOC: Pixel formats
 *
 * The formats code are formed by using 2 parts which indicated how to
 * interprete the pixel data. The first part how the components are stored:
 * in integer of 8, 16 or 32 bits, float or double. The second part
 * indicates in which order, which components must be read.
 *
 * Following this rules, the following table describes the components and
 * memory layout of some pixel format
 *
 *              format                 |  Comp.  |      memory layout
 * ------------------------------------------------------------------------
 * MM_PIXFMT_COMP_BGRA|MM_PIXFMT_UINT8 | R,G,B,A | B0 G0 R0 A0 B1 G1 R1 A1
 * MM_PIXFMT_COMP_RGBA|MM_PIXFMT_UINT8 | R,G,B,A | R0 G0 B0 A0 R1 G1 B1 A1
 * MM_PIXFMT_COMP_MONO|MM_PIXFMT_UINT16|   mono  | M0 M0 M1 M1 M2 M2 M3 M4
 * MM_PIXFMT_COMP_HSV|MM_PIXFMT_UINT16 |  H,S,V  | H0 H0 S0 S0 V0 V0 H1 H1
 *
 * MM_PIXFMT_BGRA (MM_PIXFMT_COMP_BGRA | MM_PIXFMT_UINT8) is then the same
 * as in OpenCV, but beware, it is the same as ARGB in Qt (on little endian
 * platform).
 */
#define MM_PIXFMT_DATATYPE_MASK	0x000000FF
#define MM_PIXFMT_UINT8         0x00000001
#define MM_PIXFMT_UINT16        0x00000002
#define MM_PIXFMT_UINT32        0x00000003
#define MM_PIXFMT_UINT64        0x00000004
#define MM_PIXFMT_FLOAT         0x00000005
#define MM_PIXFMT_DOUBLE        0x00000006

#define MM_PIXFMT_COMP_MASK     0x0000FF00
#define MM_PIXFMT_COMP_MONO     0x00000100
#define MM_PIXFMT_COMP_RGB      0x00000200
#define MM_PIXFMT_COMP_BGR      0x00000300
#define MM_PIXFMT_COMP_RGBA     0x00000400
#define MM_PIXFMT_COMP_BGRA     0x00000500
#define MM_PIXFMT_COMP_HSV      0x00000600
#define MM_PIXFMT_COMP_HSVA     0x00000700
#define MM_PIXFMT_COMP_HLS      0x00000800
#define MM_PIXFMT_COMP_HLSA     0x00000900

/* Usual pixel formats */
#define MM_PIXFMT_MONO8         (MM_PIXFMT_COMP_MONO | MM_PIXFMT_UINT8)
#define MM_PIXFMT_MONO16        (MM_PIXFMT_COMP_MONO | MM_PIXFMT_UINT16)
#define MM_PIXFMT_BGRA          (MM_PIXFMT_COMP_BGRA | MM_PIXFMT_UINT8)
#define MM_PIXFMT_RGBA          (MM_PIXFMT_COMP_RGBA | MM_PIXFMT_UINT8)
#define MM_PIXFMT_BGR           (MM_PIXFMT_COMP_BGR  | MM_PIXFMT_UINT8)
#define MM_PIXFMT_RGB           (MM_PIXFMT_COMP_RGB  | MM_PIXFMT_UINT8)


/**
 * struct mm_imgdesc - image buffer access description
 * @height:     Number of lines in the images
 * @width:      Number of columns in the image
 * @stride:     Number of bytes between two consecutive lines.
 * @pixformat:  Format of the pixels
 *
 * The field @stride allows to determine the starting address of each line:
 * if addr is the memory address of the first pixel of line, addr+stride is
 * the memory address of the first pixel of the next line. To have a
 * properly formatted description, this field must be as long as or longer
 * than the width multiplied by pixel size.
 */
struct mm_imgdesc {
	int height;
	int width;
	unsigned int stride;
	unsigned int pixformat;
};

#ifdef __cplusplus
extern "C" {
#endif

size_t mmimg_pixel_size(unsigned int pixel_format);
int mmimg_set_stride(struct mm_imgdesc* img, size_t alignment);
void* mmimg_alloc_buffer(const struct mm_imgdesc* img);

size_t mmimage_buffer_size(const mmimage* img);

#ifdef __cplusplus
}
#endif

#endif /* MMTYPE_H */
