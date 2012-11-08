/*
	Copyright (C) 2012  MindMaze SA
	All right reserved

	Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
*/

#include "mmgeometry.h"
#include "math.h"

mmquat from_rotmatrix3d(rotmatrix3d* mat)
{
	// Algorithm in Ken Shoemake's article in 1987 SIGGRAPH course notes
	// article "Quaternion Calculus and Fast Animation".

	mmquat out;

	float fTrace = mat->elem[0]+mat->elem[4]+mat->elem[8];
	float fRoot;

	if ( fTrace > 0.0 )
	{
		// |w| > 1/2, may as well choose w > 1/2
		fRoot = sqrt(fTrace + 1.0f);  // 2w
		out.w = 0.5f*fRoot;
		fRoot = 0.5f/fRoot;  // 1/(4w)
		out.x = (mat->elem[7]-mat->elem[5])*fRoot;
		out.y = (mat->elem[2]-mat->elem[6])*fRoot;
		out.z = (mat->elem[3]-mat->elem[1])*fRoot;
	}
	else
	{
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
		if ( mat->elem[4] > mat->elem[0] ) {
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
		if ( mat->elem[8] > mat->elem[i] ) {
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

		fRoot = sqrt(mat->elem[i]-mat->elem[j]-mat->elem[k] + 1.0f);
		float* apkQuat[3] = { &out.x, &out.y, &out.z };
		*apkQuat[a] = 0.5f*fRoot;
		fRoot = 0.5f/fRoot;
		out.w = (mat->elem[kj]-mat->elem[jk])*fRoot;
		*apkQuat[b] = (mat->elem[ji]+mat->elem[ij])*fRoot;
		*apkQuat[c] = (mat->elem[ki]+mat->elem[ik])*fRoot;
	}

	out.confidence = mat->confidence;

	return out;
}

rotmatrix3d from_quat(mmquat* q)
{
	rotmatrix3d mat;

	float fTx  = q->x+q->x;
	float fTy  = q->y+q->y;
	float fTz  = q->z+q->z;
	float fTwx = fTx*q->w;
	float fTwy = fTy*q->w;
	float fTwz = fTz*q->w;
	float fTxx = fTx*q->x;
	float fTxy = fTy*q->x;
	float fTxz = fTz*q->x;
	float fTyy = fTy*q->y;
	float fTyz = fTz*q->y;
	float fTzz = fTz*q->z;

	mat.elem[0] = 1.0f-(fTyy+fTzz);
	mat.elem[1] = fTxy-fTwz;
	mat.elem[2] = fTxz+fTwy;
	mat.elem[3] = fTxy+fTwz;
	mat.elem[4] = 1.0f-(fTxx+fTzz);
	mat.elem[5] = fTyz-fTwx;
	mat.elem[6] = fTxz-fTwy;
	mat.elem[7] = fTyz+fTwx;
	mat.elem[8] = 1.0f-(fTxx+fTyy);

	mat.confidence = q->confidence;

	return mat;
}

mmquat quat_add(const mmquat* q1, const mmquat* q2)
{
	mmquat q;
	q.w = q1->w + q2->w;
	q.x = q1->x + q2->x;
	q.y = q1->y + q2->y;
	q.z = q1->z + q2->z;
	return q;
}

mmquat quat_subst(const mmquat* q1, const mmquat* q2)
{
	mmquat q;
	q.w = q1->w - q2->w;
	q.x = q1->x - q2->x;
	q.y = q1->y - q2->y;
	q.z = q1->z - q2->z;
	return q;
}

mmquat quat_mul(const mmquat* q1, const mmquat* q2)
{
	// NOTE:  Multiplication is not generally commutative, so in most
	// cases p*q != q*p.
	mmquat q;
	q.w =q1->w * q2->w -q1->x * q2->x -q1->y * q2->y -q1->z * q2->z;
	q.x =q1->w * q2->x +q1->x * q2->w +q1->y * q2->z -q1->z * q2->y;
	q.y =q1->w * q2->y +q1->y * q2->w +q1->z * q2->x -q1->x * q2->z;
	q.z =q1->w * q2->z +q1->z * q2->w +q1->x * q2->y -q1->y * q2->x;
	return q;
}


float quat_norm(const mmquat* q)
{
	float norm = 0;
	norm += q->w*q->w;
	norm += q->x*q->x;
	norm += q->y*q->y;
	norm += q->z*q->z;
	return norm;
}


mmquat quat_inverse(const mmquat* q)
{
	mmquat out;
	float fNorm = quat_norm(q);
	if ( fNorm > 0.0 )
	{
		float fInvNorm = 1.0f/fNorm;
		out.w = q->w*fInvNorm;
		out.x = -q->x*fInvNorm;
		out.y = -q->y*fInvNorm;
		out.z = -q->z*fInvNorm;
	}
	return out;
}

epos3d epos_add(const epos3d* p1, const epos3d* p2)
{
	epos3d p;
	p.x = p1->x + p2->x;
	p.y = p1->y + p2->y;
	p.z = p1->z + p2->z;
	return p;
}

epos3d epos_subst(const epos3d* p1, const epos3d* p2)
{
	epos3d p;
	p.x = p1->x - p2->x;
	p.y = p1->y - p2->y;
	p.z = p1->z - p2->z;
	return p;
}

epos3d epos_mul(const epos3d* p, float s)
{
	epos3d out;
	out.x = p->x * s;
	out.y = p->y * s;
	out.z = p->z * s;
	return out;
}

float epos_dot(const epos3d* p1, const epos3d* p2)
{
	float out;
	out = p1->x*p2->x + p1->y*p2->y + p1->z*p2->z;
	return out;
}

epos3d epos_cross(const epos3d* p1, const epos3d* p2)
{
	epos3d p;
	p.x = p1->y*p2->z - p1->z*p2->y;
	p.y = p1->z*p2->x - p1->x*p2->z;
	p.z = p1->x*p2->y - p1->y*p2->x;
	return p;
}

float epos_norm(const epos3d* p)
{
	float out;
	out = sqrt(p->x*p->x + p->y*p->y + p->z*p->z);
	return out;
}

epos3d epos_rotate(const epos3d* p, const mmquat* q)
{
	// nVidia SDK implementation
	epos3d uv, uuv, qvec;
	qvec.x = q->x;
	qvec.y = q->y;
	qvec.z = q->z;
	uv = epos_cross(&qvec,p);
	uuv = epos_cross(&qvec,&uv);
	uv = epos_mul(&uv,2.0f * q->w);
	uuv = epos_mul(&uv,2.0f);
	uuv = epos_add(&uv, &uuv);

	return epos_add(p,&uuv);
}

epos3d plane_intersect(const mmplane* plane, const epos3d* p, const epos3d* vec)
{
	epos3d diff, out;
	float num, denum, d;
	diff = epos_subst(&plane->origin,p);
	num = epos_dot(&diff,&plane->normal);
	denum = epos_dot(vec,&plane->normal);
	d = num / denum;
	out = epos_mul(vec,d);
	return epos_add(p,&out);
}

epos3d plane_projection(const mmplane* plane, const epos3d* p)
{
	return plane_intersect(plane,p,&plane->normal);
}

epos3d get_cylinder_normal(const mmcylinder* cyl)
{
	epos3d normal;
	// Default normal (no rotation)
	normal.x = 0.f;
	normal.y = 1.f;
	normal.z = 0.f;

	// Compute normal corresponding to cylinder
	normal = epos_rotate(&normal,&cyl->rot);
}

bool pointing_to_cylinder(const mmcylinder* cyl, const epos3d* p1, const epos3d* p2)
{
	epos3d vec, intersec;
	mmplane cyl_plane;
	double dist;

	// Compute plane corresponding to cylinder
	cyl_plane.origin = cyl->pos;
	cyl_plane.normal = get_cylinder_normal(cyl);

	// Compute intersection point with cylinder
	vec = epos_subst(p1,p2);
	intersec = plane_intersect(&cyl_plane,p1,&vec);

	// Compute distance between intersection and cylinder origin
	intersec = epos_subst(&intersec,&cyl->pos);
	dist = epos_norm(&intersec);

	return dist <= cyl->radius;
}

bool collision_with_cylinder(const mmcylinder* cyl, const epos3d* p)
{
	epos3d intersec, diff;
	mmplane cyl_plane;
	double radius_dist, height_dist;

	// Compute plane corresponding to cylinder
	cyl_plane.origin = cyl->pos;
	cyl_plane.normal = get_cylinder_normal(cyl);

	// Compute projection to cylinder plane
	intersec = plane_projection(&cyl_plane,p);

	// Compute distance between intersection and cylinder origin
	diff = epos_subst(&intersec,&cyl->pos);
	radius_dist = epos_norm(&diff);

	// Compute distance between original point and intersection
	diff = epos_subst(p,&intersec);
	height_dist = epos_norm(&diff);

	return radius_dist <= cyl->radius && height_dist <= cyl->height / 2.0;
}
