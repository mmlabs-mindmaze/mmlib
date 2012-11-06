/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
*/
#ifndef MMTYPE_H
#define MMTYPE_H

/* Image depth definitions */
#define MM_DEPTH_SIGN 0x80000000
#define MM_DEPTH_1U     1
#define MM_DEPTH_8U     8
#define MM_DEPTH_16U   16
#define MM_DEPTH_32F   32
#define MM_DEPTH_8S  (MM_DEPTH_SIGN| 8)
#define MM_DEPTH_16S (MM_DEPTH_SIGN|16)
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
	int nChannels;    /* Most functions support 1,2,3 or 4 channels */
	int depth;        /* Pixel depth in bits: U for unsigned, S for
	                     signed and F for floating point */
	int width;        /* Image width in pixels. */
	int height;       /* Image height in pixels. */
	void *imageData;  /* Pointer to image data. */
} mmimage;

typedef struct mmheadinfo {
	float x;          /* Head center in image coordinates. */
	float y;
	float width;      /* Size of the head, representing ellipse axis. */
	float height;
	float angle;      /* Rotation of the head in the image plane. */
} mmheadinfo;

typedef struct epos3d {
	float x;
	float y;
	float z;
	int confidence;	  /* Confidence in the estimation */
} epos3d;

typedef struct rotmatrix3d {
	float elem[9];
	int confidence;
} rotmatrix;

#endif /* MMTYPE_H */

