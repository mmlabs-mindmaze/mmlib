/*
   @mindmaze_header@
*/
#ifndef MMTYPE_H
#define MMTYPE_H

#include <stddef.h>
#include "mmpredefs.h"

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


/**************************************************************************
 *                                                                        *
 *                             Image data types                           *
 *                                                                        *
 **************************************************************************/

/**
 * DOC: Pixel formats
 *
 * The formats code are formed by using 2 parts which indicated how to
 * interpret the pixel data. The first part how the components are stored:
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

MMLIB_API size_t mmimg_pixel_size(unsigned int pixel_format);
MMLIB_API int mmimg_set_stride(struct mm_imgdesc* img, size_t alignment);
MMLIB_API void* mmimg_alloc_buffer(const struct mm_imgdesc* img);
MMLIB_API void mmimg_free_buffer(void* img_buff);

MMLIB_API size_t mmimage_buffer_size(const mmimage* img);

#ifdef __cplusplus
}
#endif


/**************************************************************************
 *                                                                        *
 *                 Calibration interfaces definitions                     *
 *                                                                        *
 **************************************************************************/

#define MM_CALIB_RUN            -1
#define MM_CALIB_CANCELLED      -2

/* Element types */
#define MM_CELT_BUTTON          0
#define MM_CELT_TEXT            1
#define MM_CELT_INDICATOR       2
#define MM_CELT_BAR             3

/* Element states */
#define MM_CELTST_OK            0x00    // state
#define MM_CELTST_WARN          0x01    // state
#define MM_CELTST_FAIL          0x02    // state
#define MM_CELTST_LOW           0x10    // state
#define MM_CELTST_HIGH          0x11    // state
#define MM_CELTST_DISABLE       0x04    // flag
#define MM_CELTST_INVISIBLE     0x08    // flag

/* Soft limits
 If those limits are not respected, the client will not crashed. However the
 calibrated module is not ensured that the panel will display fully every
 label or element beyond the limits.
 */
#define MM_CALIB_BUTTON_RIGHT   0
#define MM_CALIB_MINBUTTON      1
#define MM_CALIB_MAXBUTTON      4
#define MM_CALIB_MAXINDICATOR   8
#define MM_CALIB_MAXBAR         1
#define MM_CALIB_MAXTEXT        3
#define MM_CALIB_MAXIMG         1
#define MM_CALIB_INDIC_MAXLEN   32
#define MM_CALIB_BUTTON_MAXLEN  16
#define MM_CALIB_TEXT_MAXLEN    128
#define MM_CALIB_BAR_MAXLEN     32
#define MM_CALIB_TITLE_MAXLEN   64
#define MM_CALIB_MAXVALUE       INT_MAX


/**
 * Description of a new calibration panel
 */
struct mm_calib_paneldesc {
	int num_text;           //! number of text element
	int num_button;         //! number of button element (minimum MM_CALIB_MINBUTTON)
	int num_indicator;      //! number of indicator
	int num_bar;            //! number of progress bar element
	int num_img;            //! number of image slot
	const char* title;      //! title to display in the new calibration panel
};


/**
 * struct mm_calibration_cb - Callback to the GUI for the calibration
 */
struct mm_calibration_cb {
	// Pointer to the pointer to supply in callbacks
	void* data;

	/**
	 * create_img() - configure a new image be available in gui in specified slot
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 * @desc:       image format descriptor the image that will be passed
	 * @slot:       image slot to configure
	 *
	 * Return: pointer to the image buffer to be used for the new image update
	 */
	void* (*create_img)(void* ptr, const struct mm_imgdesc* des, int slot);

	/**
	 * update_img() - signal that a new image content is available in the specified slot
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 * @slot:       image slot to update
	 *
	 * Return: pointer to the image buffer to be used for the new image update
	 */
	void* (*update_img)(void* ptr, int slot);

	/**
	 * update_label() - update label of specified calibration element
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 * @elt_type:   element type to update (MM_CELT_*)
	 * @slot:       slot of the element of type @elt_type to update
	 * @label:      new label to display
	 */
	void  (*update_label)(void* ptr, int elt_type, int slot, const char* label);

	/**
	 * update_state() - update state of specified calibration element
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 * @elt_type:   element type to update
	 * @slot:       slot of the element of type @elt_type to update
	 * @state:      state to change (MM_CELTST_* with mask of MM_CELTST_DISABLE and/or MM_CELTST_INVISIBLE)
	 */
	void  (*update_state)(void* ptr, int elt_type, int slot, int state);

	/**
	 * update_value() - update value displayed in specified calibration element
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 * @elt_type:   element type to update
	 * @slot:       slot of the element of type @elt_type to update
	 * @value:      value to change
	 */
	void  (*update_value)(void* ptr, int elt_type, int slot, int value);

	/**
	 * define_panel() - discard content of previous calibration panel and setup a new one
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 * @paneldesc:  description of the new panel
	 */
	void  (*define_panel)(void* ptr, const struct mm_calib_paneldesc* paneldesc);

	/**
	 * check_answer() - Get the response of calibration feedback
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 *
	 * This callback allows the module to be calibrated to inspect if a
	 * button has been clicked since the last call to this callback. The
	 * callback will return:
	 * - MM_CALIB_CANCELLED: if calibration has been cancelled.
	 * - the button index: if one button has been clicked and that has
	 * not been reported since the last call to this callback.
	 * - MM_CALIB_RUN: if not button has been clicked since last call
	 * and the calibration has not been cancelled.
	 *
	 * Return: the index of the last button clicked, MM_CALIB_RUN or
	 * MM_CALIB_CANCELLED.
	 */
	int   (*check_answer)(void* ptr);

	/**
	 * check_click() - Get the pointer click or touch of an image slot
	 * @ptr:        user provided pointer @mm_calibration_cb.data
	 * @img_slot:   image slot of the click
	 * @x:          pointer to value receiving the position abscissa
	 * @y:          pointer to value receiving the position ordinate
	 *
	 * Similar to check_answer(), get the position of the last click (or
	 * touch) on any of the image slots.
	 *
	 * Return: 0 if no new click is available, 1 if a new click has been
	 * reported on one image slot.
	 */
	int   (*check_click)(void* ptr, int* img_slot, float* x, float* y);
};


#endif /* MMTYPE_H */
