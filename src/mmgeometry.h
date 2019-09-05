/*
 * @mindmaze_header@
 */
#ifndef MMGEOMETRY_H
#define MMGEOMETRY_H

#include <mmtype.h>
#include <string.h>
#include <math.h>
#include "mmpredefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GCC 2.95 and later have "__restrict"; C99 compilers have
 * "restrict", and "configure" may have defined "restrict".  */
#if !(199901L <= __STDC_VERSION__ || defined restrict)
# if 2 < __GNUC__ || (2 == __GNUC__ && 95 <= __GNUC_MINOR__) \
        || defined __restrict
#  define restrict __restrict
# else
#  define restrict
# endif
#endif

#ifndef DEPRECATED
# if 3 < __GNUC__ || (3 == __GNUC__ && 1 <= __GNUC_MINOR__)
# define DEPRECATED __attribute__((deprecated))
#else
# define DEPRECATED
#endif
#endif

// Conversion between quaternion and rotation matrix
MMLIB_API float* mm_quat_from_mat3(float* restrict q, const float* restrict m);
MMLIB_API float* mm_mat3_from_quat(float* restrict m, const float* restrict q);

// Use the above function instead of these one
MMLIB_API float* mm_quat_from_mat(float* restrict q, const float* restrict
                                  m) DEPRECATED;
MMLIB_API float* mm_mat_from_quat(float* restrict m, const float* restrict
                                  q) DEPRECATED;

// Conversion between quaternion, rotation matrix and angle axis
MMLIB_API float mm_aaxis_from_mat3(float* restrict axis,
                                   const float* restrict m);

// 3D Vector specific operations
static inline
float* mm_add(float* restrict v1, const float* restrict v2)
{
	v1[0] += v2[0];
	v1[1] += v2[1];
	v1[2] += v2[2];
	return v1;
}

static inline
float* mm_subst(float* restrict v1, const float* restrict v2)
{
	v1[0] -= v2[0];
	v1[1] -= v2[1];
	v1[2] -= v2[2];
	return v1;
}

static inline
float* mm_mul(float * v, float s)
{
	v[0] *= s;
	v[1] *= s;
	v[2] *= s;
	return v;
}

static inline
float mm_dot(const float* restrict v1, const float* restrict v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

static inline
float mm_norm(const float * v)
{
	return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

MMLIB_API float* mm_cross(float* restrict v1, const float* restrict v2);
MMLIB_API float* mm_rotate(float* restrict v, const float* restrict q);

// Quaternion specific opereations
static inline
float quat_norm(const float * q)
{
	return q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
}

static inline
float* quat_conjugate(float * q)
{
	q[1] = -q[1];
	q[2] = -q[2];
	q[3] = -q[3];
	return q;
}

static inline
float* quat_inverse(float * q)
{
	float norminv = 1.0f / quat_norm(q);
	q[0] *= norminv;
	q[1] *= -norminv;
	q[2] *= -norminv;
	q[3] *= -norminv;
	return q;
}

MMLIB_API float* quat_mul(float* restrict q1, const float* restrict q2);

// Plane operations (plane defined as ax + bx + cy + d = 0)
MMLIB_API float* plane_from_point(float* restrict plane,
                                  const float* restrict p);
MMLIB_API float plane_distance(const float* restrict p,
                               const float* restrict plane);
MMLIB_API float* plane_intersect(float* restrict p, const float * v,
                                 const float * plane);
MMLIB_API float* plane_projection(float* restrict p,
                                  const float* restrict plane);

#ifdef __cplusplus
}
#endif

#endif /* MMGEOMETRY_H */
