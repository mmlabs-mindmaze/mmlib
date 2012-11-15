/*
	Copyright (C) 2012  MindMaze SA
	All right reserved

	Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
	        Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#ifndef MMGEOMETRY_H
#define MMGEOMETRY_H

#include <mmtype.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GCC 2.95 and later have "__restrict"; C99 compilers have
   "restrict", and "configure" may have defined "restrict".  */
#ifndef __restrict
# if ! (2 < __GNUC__ || (2 == __GNUC__ && 95 <= __GNUC_MINOR__))
#  if defined restrict || 199901L <= __STDC_VERSION__
#   define __restrict restrict
#  else
#   define __restrict
#  endif
# endif
#endif

// Conversion between quaternion and rotation matrix
mmquat from_rotmatrix3d(rotmatrix3d* mat);
rotmatrix3d from_quat(mmquat* q);

// 3D Vector specific operations
float* mm_add(float *__restrict v1, const float *__restrict v2);
float* mm_subst(float *__restrict v1, const float *__restrict v2);
float* mm_mul(float *v, float s);
float mm_dot(const float *__restrict v1, const float *__restrict v2);
float mm_norm(const float *v);
float* mm_cross(float *__restrict v1, const float *__restrict v2);
float* mm_rotate(float *__restrict v, const float *__restrict q);

// Quaternion specific opereations
float* quat_conjugate(float *q);
float* quat_mul(float *__restrict q1, const float *__restrict q2);
float* quat_inverse(float* q);

// Plane operations (plane defined as ax + bx + cy + d = 0)
float* plane_from_point(float *__restrict plane, const float *__restrict p);
float plane_distance(const float *__restrict p,
                     const float *__restrict plane);
float* plane_intersect(float *__restrict p, const float *v,
                                            const float *plane);
float* plane_projection(float *__restrict p, const float *__restrict plane);

// Cylinder operations
int pointing_to_cylinder(const mmcylinder* cyl, const float* p1, const float* p2);
int collision_with_cylinder(const mmcylinder* cyl, const float* p);

#ifdef __cplusplus
}
#endif

#endif /* MMGEOMETRY_H */
