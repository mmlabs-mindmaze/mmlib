/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmgeometry.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define FLOAT_TOLERANCE 0.001

int is_equal(const float *v1, const float *v2, int size)
{
	int i;
	float sum = 0;
	for (i = 0; i < size; i++)
		sum += fabs(v1[i] - v2[i]);
	if (sum < FLOAT_TOLERANCE)
		return 1;
	else
		return 0;
}

int main(void)
{
	float v[] = { 1.f, 2.f, 3.f };
	const float vcheck[] = { 1.f, 2.f, 3.f };

	mm_add(v, vcheck, 3);
	mm_subst(v, vcheck, 3);

	if (!is_equal(v, vcheck, 3)) {
		printf("add/sub test failure.\n");
		return EXIT_FAILURE;
	}

	mm_mul(v, 4.f, 3);
	mm_mul(v, 0.25f, 3);

	if (!is_equal(v, vcheck, 3)) {
		printf("mm_mul test failure.\n");
		return EXIT_FAILURE;
	}

	float val;
	val = mm_norm(vcheck, 3);
	val *= val;
	val -= mm_dot(vcheck, vcheck, 3);

	if (fabs(val) > FLOAT_TOLERANCE) {
		printf("dot/norm test failure.\n");
		return EXIT_FAILURE;
	}

	float sq2 = sqrt(0.5);
	float vx1[] = { 1.f, 0.f, 0.f };
	float vx2[] = { 1.f, 0.f, 0.f };
	float vy[] = { 0.f, 1.f, 0.f };
	float roty90[] = { sq2, 0.f, -sq2, 0.f };

	mm_cross(vx1, vy);
	mm_rotate(vx2, roty90);

	if (!is_equal(vx1, vx2, 3)) {
		printf("cross/rotate test failure.\n");
		return EXIT_FAILURE;
	}

	float point[] = { 2.0, 1.0, 1.0 }, proj[] = {0.0, 1.0, 1.0};
	float planexy[] = { 1.0, 0.0, 0.0, 0.0 };
	float dist_check = 2.0, dist;

	dist = plane_distance(point, planexy);
	plane_projection(point, planexy);

	if (!is_equal(point, proj, 3) || fabs(dist-dist_check) > FLOAT_TOLERANCE) {
		printf("plane test failure.\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
