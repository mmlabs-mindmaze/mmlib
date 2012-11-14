/*
	Copyright (C) 2012  MindMaze SA
	All right reserved

	Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
*/

#include "mmgeometry.h"
#include "math.h"
#include "string.h"

// ---------------------------------------------------- //
// ----- Quaternion <-> Rotation matrix conversion ---- //
// ---------------------------------------------------- //

mmquat from_rotmatrix3d(rotmatrix3d* mat)
{
	// Algorithm in Ken Shoemake's article in 1987 SIGGRAPH course notes
	// article "Quaternion Calculus and Fast Animation".

	mmquat out;

	float fTrace = mat->elem[0] + mat->elem[4] + mat->elem[8];
	float fRoot;

	if (fTrace > 0.0) {
		// |w| > 1/2, may as well choose w > 1/2
		fRoot = sqrt(fTrace + 1.0f);	// 2w
		out.v[0] = 0.5f * fRoot;
		fRoot = 0.5f / fRoot;	// 1/(4w)
		out.v[1] = (mat->elem[7] - mat->elem[5]) * fRoot;
		out.v[2] = (mat->elem[2] - mat->elem[6]) * fRoot;
		out.v[3] = (mat->elem[3] - mat->elem[1]) * fRoot;
	} else {
		// |w| <= 1/2
		int a = 0;
		int b = 1;
		int c = 2;
		int i = 0;
		int ij = 1;
		int ik = 2;
		int ji = 3;
		int j = 4;
		int jk = 5;
		int ki = 6;
		int kj = 7;
		int k = 8;
		if (mat->elem[4] > mat->elem[0]) {
			a = 1;
			b = 2;
			c = 0;
			i = 4;
			ij = 5;
			ik = 3;
			ji = 7;
			j = 8;
			jk = 6;
			ki = 1;
			kj = 2;
			k = 0;
		}
		if (mat->elem[8] > mat->elem[i]) {
			a = 2;
			b = 0;
			c = 1;
			i = 8;
			ij = 6;
			ik = 7;
			ji = 2;
			j = 0;
			jk = 1;
			ki = 5;
			kj = 3;
			k = 4;
		}

		fRoot = sqrt(mat->elem[i] - mat->elem[j] - mat->elem[k] + 1.0f);
		float *apkQuat[3] = { &out.v[1], &out.v[2], &out.v[3] };
		*apkQuat[a] = 0.5f * fRoot;
		fRoot = 0.5f / fRoot;
		out.v[0] = (mat->elem[kj] - mat->elem[jk]) * fRoot;
		*apkQuat[b] = (mat->elem[ji] + mat->elem[ij]) * fRoot;
		*apkQuat[c] = (mat->elem[ki] + mat->elem[ik]) * fRoot;
	}

	out.confidence = mat->confidence;

	return out;
}

rotmatrix3d from_quat(mmquat * q)
{
	rotmatrix3d mat;

	float fTx = q->v[1] + q->v[1];
	float fTy = q->v[2] + q->v[2];
	float fTz = q->v[3] + q->v[3];
	float fTwx = fTx * q->v[0];
	float fTwy = fTy * q->v[0];
	float fTwz = fTz * q->v[0];
	float fTxx = fTx * q->v[1];
	float fTxy = fTy * q->v[1];
	float fTxz = fTz * q->v[1];
	float fTyy = fTy * q->v[2];
	float fTyz = fTz * q->v[2];
	float fTzz = fTz * q->v[3];

	mat.elem[0] = 1.0f - (fTyy + fTzz);
	mat.elem[1] = fTxy - fTwz;
	mat.elem[2] = fTxz + fTwy;
	mat.elem[3] = fTxy + fTwz;
	mat.elem[4] = 1.0f - (fTxx + fTzz);
	mat.elem[5] = fTyz - fTwx;
	mat.elem[6] = fTxz - fTwy;
	mat.elem[7] = fTyz + fTwx;
	mat.elem[8] = 1.0f - (fTxx + fTyy);

	mat.confidence = q->confidence;

	return mat;
}

// ---------------------------- //
// ---- Generic Operations ---- //
// ---------------------------- //

float *mm_add(float *v1, const float *v2, int size)
{
	int i;
	for (i = 0; i < size; i++)
		v1[i] += v2[i];
	return v1;
}

float *mm_subst(float *v1, const float *v2, int size)
{
	int i;
	for (i = 0; i < size; i++)
		v1[i] -= v2[i];
	return v1;
}

float *mm_mul(float *v, float s, int size)
{
	int i;
	for (i = 0; i < size; i++)
		v[i] *= s;
	return v;
}

float mm_dot(const float *v1, const float *v2, int size)
{
	float dot = 0;
	int i;
	for (i = 0; i < size; i++)
		dot += v1[i] * v2[i];
	return dot;
}

float mm_norm(const float *v, int size)
{
	float norm = 0;
	int i;
	for (i = 0; i < size; i++)
		norm += v[i] * v[i];
	return sqrt(norm);
}

// ------------------------------ //
// ---- 3D Vector Operations ---- //
// ------------------------------ //

float *mm_cross(float *v1, const float *v2)
{
	float out[3];
	out[0] = v1[1]*v2[2] - v1[2]*v2[1];
	out[1] = v1[2]*v2[0] - v1[0]*v2[2];
	out[2] = v1[0]*v2[1] - v1[1]*v2[0];
	memcpy(v1, out, sizeof(float) * 3);
	return v1;
}

float *mm_rotate(float *v, const float *q)
{
	// nVidia SDK implementation
	float uv[3], uuv[3];
	memcpy(uv, q + 1, sizeof(float) * 3);
	memcpy(uuv, q + 1, sizeof(float) * 3);
	mm_cross(uv, v);
	mm_cross(uuv, uv);
	mm_mul(uv, 2.0f * q[0], 3);
	mm_mul(uuv, 2.0f, 3);
	mm_add(v, mm_add(uuv, uv, 3), 3);
	return v;
}

// -------------------------------- //
// ----- Quaternion operations ---- //
// -------------------------------- //

float *quat_mul(float *q1, const float *q2)
{
	float out[4];
	out[0] = q1[0]*q2[0] - q1[1]*q2[1] - q1[2]*q2[2] - q1[3]*q2[3];
	out[1] = q1[0]*q2[1] + q1[1]*q2[0] + q1[2]*q2[3] - q1[3]*q2[2];
	out[2] = q1[0]*q2[2] + q1[2]*q2[0] + q1[3]*q2[1] - q1[1]*q2[3];
	out[3] = q1[0]*q2[3] + q1[3]*q2[0] + q1[1]*q2[2] - q1[2]*q2[1];
	memcpy(q1, out, sizeof(float) * 4);
	return q1;
}

float *quat_conjugate(float *q)
{
	q[1] = -q[1];
	q[2] = -q[2];
	q[3] = -q[3];
	return q;
}

float *quat_inverse(float *q)
{
	float fNorm = mm_norm(q, 4);
	if (fNorm > 0.0) {
		float fInvNorm = 1.0f / (fNorm * fNorm);
		q[0] *= fInvNorm;
		q[1] *= -fInvNorm;
		q[2] *= -fInvNorm;
		q[3] *= -fInvNorm;
	}
	return q;
}

// -------------------------- //
// ---- Plane operations ---- //
// -------------------------- //

float *plane_from_point(float *plane, const float *p)
{
	plane[3] = -mm_dot(plane, p, 3);
	return plane;
}

float plane_distance(const float *p, const float *plane)
{
	return fabs(mm_dot(plane, p, 3) + plane[3]) / mm_norm(plane, 3);
}

float *plane_intersect(float *p, const float *v, const float *plane)
{
	float d, v2[3];
	d = -(mm_dot(plane, p, 3) + plane[3]) / mm_dot(plane, v, 3);
	memcpy(v2, v, sizeof(float) * 3);
	mm_add(p, mm_mul(v2, d, 3), 3);

	return p;
}

float *plane_projection(float *p, const float *plane)
{
	return plane_intersect(p, plane, plane);
}

// ----------------------------- //
// ---- Cylinder operations ---- //
// ----------------------------- //

bool pointing_to_cylinder(const mmcylinder * cyl, const float *p1,
			  const float *p2)
{
	float cyl_plane[] = { 0.f, 1.f, 0.f, 0.f };
	float p[3], v[3];
	float dist;

	// Compute cylinder plane
	mm_rotate(cyl_plane, cyl->rot);
	plane_from_point(cyl_plane, cyl->pos);

	// Compute intersection point with cylinder plane
	memcpy(p, p1, sizeof(float) * 3);
	memcpy(v, p1, sizeof(float) * 3);
	mm_subst(v, p2, 3);
	plane_intersect(p, v, cyl_plane);

	// Compute distance between intersection and cylinder origin
	dist = mm_norm(mm_subst(p, cyl->pos, 3), 3);

	return dist <= cyl->radius;
}

bool collision_with_cylinder(const mmcylinder * cyl, const float *p)
{
	float cyl_plane[] = { 0.f, 1.f, 0.f, 0.f };
	float intersec[3];
	float radius_dist, height_dist;

	// Compute cylinder plane
	mm_rotate(cyl_plane, cyl->rot);
	plane_from_point(cyl_plane, cyl->pos);

	// Compute projection to cylinder plane
	memcpy(intersec, p, sizeof(float) * 3);
	plane_projection(intersec, cyl_plane);

	// Compute distance between intersection and cylinder origin
	radius_dist = mm_norm(mm_subst(intersec, cyl->pos, 3), 3);

	// Compute distance between original point and cylinder plane
	height_dist = plane_distance(p, cyl_plane);

	return radius_dist <= cyl->radius && height_dist <= cyl->height / 2.0;
}
