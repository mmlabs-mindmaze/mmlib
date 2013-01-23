/*
	Copyright (C) 2012  MindMaze SA
	All right reserved

	Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
	        Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#ifndef MMGEOMETRY_H
#define MMGEOMETRY_H

#include <mmtype.h>
#include <string.h>
#include <math.h>

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
float* mm_quat_from_mat(float *__restrict q, const float *__restrict m);
float* mm_mat_from_quat(float *__restrict m, const float *__restrict q);

// 3D Vector specific operations
static inline
float* mm_add(float *__restrict v1, const float *__restrict v2)
{
	v1[0] += v2[0];
	v1[1] += v2[1];
	v1[2] += v2[2];
	return v1;
}

static inline
float* mm_subst(float *__restrict v1, const float *__restrict v2)
{
	v1[0] -= v2[0];
	v1[1] -= v2[1];
	v1[2] -= v2[2];
	return v1;
}

static inline
float* mm_mul(float *v, float s)
{
	v[0] *= s;
	v[1] *= s;
	v[2] *= s;
	return v;
}

static inline
float mm_dot(const float *__restrict v1, const float *__restrict v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

static inline
float mm_norm(const float *v)
{
	return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

float* mm_cross(float *__restrict v1, const float *__restrict v2);
float* mm_rotate(float *__restrict v, const float *__restrict q);

// Quaternion specific opereations
static inline
float quat_norm(const float *q)
{
	return q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
}

static inline
float *quat_conjugate(float *q)
{
	q[1] = -q[1];
	q[2] = -q[2];
	q[3] = -q[3];
	return q;
}

static inline
float *quat_inverse(float *q)
{
	float norminv = 1.0f / quat_norm(q);
	q[0] *= norminv;
	q[1] *= -norminv;
	q[2] *= -norminv;
	q[3] *= -norminv;
	return q;
}

float* quat_mul(float *__restrict q1, const float *__restrict q2);

// Plane operations (plane defined as ax + bx + cy + d = 0)
float* plane_from_point(float *__restrict plane, const float *__restrict p);
float plane_distance(const float *__restrict p,
                     const float *__restrict plane);
float* plane_intersect(float *__restrict p, const float *v,
                                            const float *plane);
float* plane_projection(float *__restrict p, const float *__restrict plane);

#ifdef __cplusplus
}
#endif

#endif /* MMGEOMETRY_H */
