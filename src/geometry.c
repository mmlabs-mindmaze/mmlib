/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmgeometry.h"
#include <math.h>
#include <string.h>

// ---------------------------------------------------- //
// ----- Quaternion <-> Rotation matrix conversion ---- //
// ---------------------------------------------------- //
API_EXPORTED
float* mm_quat_from_mat3(float *restrict quat, const float *restrict mat)
{
	// Algorithm in Ken Shoemake's article in 1987 SIGGRAPH course notes
	// article "Quaternion Calculus and Fast Animation".

	float fTrace = mat[0] + mat[4] + mat[8];
	float fRoot;

	if (fTrace > 0.0) {
		// |w| > 1/2, may as well choose w > 1/2
		fRoot = sqrtf(fTrace + 1.0f);	// 2w
		quat[0] = 0.5f * fRoot;
		fRoot = 0.5f / fRoot;	// 1/(4w)
		quat[1] = (mat[7] - mat[5]) * fRoot;
		quat[2] = (mat[2] - mat[6]) * fRoot;
		quat[3] = (mat[3] - mat[1]) * fRoot;
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
		if (mat[4] > mat[0]) {
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
		if (mat[8] > mat[i]) {
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

		fRoot = sqrtf(mat[i] - mat[j] - mat[k] + 1.0f);
		float *apkQuat[3] = { quat+1, quat+2, quat+3 };
		*apkQuat[a] = 0.5f * fRoot;
		fRoot = 0.5f / fRoot;
		quat[0] = (mat[kj] - mat[jk]) * fRoot;
		*apkQuat[b] = (mat[ji] + mat[ij]) * fRoot;
		*apkQuat[c] = (mat[ki] + mat[ik]) * fRoot;
	}

	return quat;
}


API_EXPORTED
float* mm_quat_from_mat(float *restrict quat, const float *restrict mat)
{
	return mm_quat_from_mat3(quat, mat);
}


API_EXPORTED
float* mm_mat3_from_quat(float *restrict mat, const float *restrict quat)
{
	float fTx = quat[1] + quat[1];
	float fTy = quat[2] + quat[2];
	float fTz = quat[3] + quat[3];
	float fTwx = fTx * quat[0];
	float fTwy = fTy * quat[0];
	float fTwz = fTz * quat[0];
	float fTxx = fTx * quat[1];
	float fTxy = fTy * quat[1];
	float fTxz = fTz * quat[1];
	float fTyy = fTy * quat[2];
	float fTyz = fTz * quat[2];
	float fTzz = fTz * quat[3];

	mat[0] = 1.0f - (fTyy + fTzz);
	mat[1] = fTxy - fTwz;
	mat[2] = fTxz + fTwy;
	mat[3] = fTxy + fTwz;
	mat[4] = 1.0f - (fTxx + fTzz);
	mat[5] = fTyz - fTwx;
	mat[6] = fTxz - fTwy;
	mat[7] = fTyz + fTwx;
	mat[8] = 1.0f - (fTxx + fTyy);

	return mat;
}


API_EXPORTED
float* mm_mat_from_quat(float *restrict mat, const float *restrict quat)
{
	return mm_mat3_from_quat(mat, quat);
}


API_EXPORTED
float mm_aaxis_from_mat3(float *restrict axis, const float *restrict m)
{
	float q[4];
	float sc;

	mm_quat_from_mat3(q, m);

	sc = mm_norm(q+1);
	if (sc > 1e-6) {
		axis[0] = q[1] / sc;
		axis[1] = q[2] / sc;
		axis[2] = q[3] / sc;
	} else {
		axis[0] = 1.0f;
		axis[1] = 0.0f;
		axis[2] = 0.0f;
	}

	return 2*acosf(q[0]);
}

// ------------------------------ //
// ---- 3D Vector Operations ---- //
// ------------------------------ //
API_EXPORTED
float* mm_cross(float *restrict v1, const float *restrict v2)
{
	float out[3];
	out[0] = v1[1]*v2[2] - v1[2]*v2[1];
	out[1] = v1[2]*v2[0] - v1[0]*v2[2];
	out[2] = v1[0]*v2[1] - v1[1]*v2[0];
	memcpy(v1, out, sizeof(float) * 3);
	return v1;
}

API_EXPORTED
float *mm_rotate(float *restrict v, const float *restrict q)
{
	// nVidia SDK implementation
	float uv[3], uuv[3];
	memcpy(uv, q + 1, sizeof(uv));
	memcpy(uuv, q + 1, sizeof(uuv));
	mm_cross(uv, v);
	mm_cross(uuv, uv);
	mm_mul(uv, 2.0f * q[0]);
	mm_mul(uuv, 2.0f);
	mm_add(v, mm_add(uuv, uv));
	return v;
}

// -------------------------------- //
// ----- Quaternion operations ---- //
// -------------------------------- //

API_EXPORTED
float *quat_mul(float *restrict q1, const float *restrict q2)
{
	float out[4];
	out[0] = q1[0]*q2[0] - q1[1]*q2[1] - q1[2]*q2[2] - q1[3]*q2[3];
	out[1] = q1[0]*q2[1] + q1[1]*q2[0] + q1[2]*q2[3] - q1[3]*q2[2];
	out[2] = q1[0]*q2[2] + q1[2]*q2[0] + q1[3]*q2[1] - q1[1]*q2[3];
	out[3] = q1[0]*q2[3] + q1[3]*q2[0] + q1[1]*q2[2] - q1[2]*q2[1];
	memcpy(q1, out, sizeof(out));
	return q1;
}

// -------------------------- //
// ---- Plane operations ---- //
// -------------------------- //

API_EXPORTED
float *plane_from_point(float *restrict plane, const float *restrict p)
{
	plane[3] = -mm_dot(plane, p);
	return plane;
}

API_EXPORTED
float plane_distance(const float *restrict p, const float *restrict plane)
{
	return fabs(mm_dot(plane, p) + plane[3]) / mm_norm(plane);
}

API_EXPORTED
float *plane_intersect(float *restrict p, const float *v,
                                            const float *plane)
{
	float d, v2[3];
	d = -(mm_dot(plane, p) + plane[3]) / mm_dot(plane, v);
	memcpy(v2, v, sizeof(v2));
	mm_add(p, mm_mul(v2, d));

	return p;
}

API_EXPORTED
float *plane_projection(float *restrict p, const float *restrict plane)
{
	return plane_intersect(p, plane, plane);
}

