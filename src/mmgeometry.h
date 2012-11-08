/*
	Copyright (C) 2012  MindMaze SA
	All right reserved

	Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
*/
#ifndef MMGEOMETRY_H
#define MMGEOMETRY_H

#include "mmtype.h"
#include "stdbool.h"

mmquat from_rotmatrix3d(rotmatrix3d* mat);
rotmatrix3d from_quat(mmquat* q);

mmquat quat_add(const mmquat* q1, const mmquat* q2);
mmquat quat_subst(const mmquat* q1, const mmquat* q2);
mmquat quat_mul(const mmquat* q1, const mmquat* q2);
float quat_norm(const mmquat* q);
mmquat quat_inverse(const mmquat* q);

epos3d epos_add(const epos3d* p1, const epos3d* p2);
epos3d epos_subst(const epos3d* p1, const epos3d* p2);
epos3d epos_mul(const epos3d* p, float s);
float epos_dot(const epos3d* p1, const epos3d* p2);
epos3d epos_cross(const epos3d* p1, const epos3d* p2);
float epos_norm(const epos3d* p);
epos3d epos_rotate(const epos3d* p, const mmquat* q);

epos3d plane_intersect(const mmplane* plane, const epos3d* p, const epos3d* vec);
epos3d plane_projection(const mmplane* plane, const epos3d* p);

epos3d get_cylinder_normal(const mmcylinder* cyl);
bool pointing_to_cylinder(const mmcylinder* cyl, const epos3d* p1, const epos3d* p2);
bool collision_with_cylinder(const mmcylinder* cyl, const epos3d* p);

#endif /* MMGEOMETRY_H */
