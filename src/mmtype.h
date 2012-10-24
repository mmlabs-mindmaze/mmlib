/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
*/
#ifndef MMTYPE_H
#define MMTYPE_H

#define MPP_DEPTH_SIGN 0x80000000

#define MPP_DEPTH_1U     1
#define MPP_DEPTH_8U     8
#define MPP_DEPTH_16U   16
#define MPP_DEPTH_32F   32

#define MPP_DEPTH_8S  (MPP_DEPTH_SIGN| 8)
#define MPP_DEPTH_16S (MPP_DEPTH_SIGN|16)
#define MPP_DEPTH_32S (MPP_DEPTH_SIGN|32)

typedef struct mppimage
{
    int  nChannels;   /* Most functions support 1,2,3 or 4 channels */
    int  depth;       /* Pixel depth in bits: U for unsigned, S for signed
                         and F for floating point */
    int  width;       /* Image width in pixels. */
    int  height;      /* Image height in pixels. */
    void *imageData;  /* Pointer to image data. */
}
mppimage;

typedef struct mppheadinfo
{
    float x;          /* Head center in image coordinates. */
    float y;
    float width;      /* Size of the head, representing ellipse axis. */
    float height;
    float angle;      /* Rotation of the head in the image plane. */
}
mppheadinfo;

#endif /* MMTYPE_H */
