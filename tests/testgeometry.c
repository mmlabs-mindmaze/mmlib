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
#include <check.h>

#define FLOAT_TOLERANCE 0.001

static const float vcheck[] = { 1.f, 2.f, 3.f };

static
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


START_TEST(add_sub_test)
{
	float v[] = { 1.f, 2.f, 3.f };
	mm_add(v, vcheck);
	mm_subst(v, vcheck);
	ck_assert(is_equal(v, vcheck,3));
}
END_TEST


START_TEST(multiply_test)
{
	float v[] = { 1.f, 2.f, 3.f };
	mm_mul(v, 4.f);
	mm_mul(v, 0.25f);
	ck_assert(is_equal(v, vcheck,3));
}
END_TEST


START_TEST(norm_dot_test)
{
	float val;
	val = mm_norm(vcheck);
	val *= val;
	val -= mm_dot(vcheck, vcheck);
	ck_assert(fabs(val) < FLOAT_TOLERANCE);
}
END_TEST


START_TEST(cross_rotate_test)
{
	float sq2 = sqrt(0.5);
	float vx1[] = { 1.f, 0.f, 0.f };
	float vx2[] = { 1.f, 0.f, 0.f };
	float vy[] = { 0.f, 1.f, 0.f };
	float roty90[] = { sq2, 0.f, -sq2, 0.f };
	mm_cross(vx1, vy);
	mm_rotate(vx2, roty90);
	ck_assert(is_equal(vx1, vx2, 3));
}
END_TEST


START_TEST(plane_projection_test)
{
	float point[] = { 2.0, 1.0, 1.0 }, proj[] = {0.0, 1.0, 1.0};
	float planexy[] = { 1.0, 0.0, 0.0, 0.0 };
	float dist_check = 2.0, dist;
	dist = plane_distance(point, planexy);
	plane_projection(point, planexy);
	ck_assert(is_equal(point, proj, 3) 
	          && fabs(dist-dist_check) < FLOAT_TOLERANCE);
}
END_TEST

static
Suite* geometry_suite(void)
{
	Suite *s = suite_create("Geometry");

	/* Core test case */
	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, add_sub_test);
	tcase_add_test(tc_core, multiply_test);
	tcase_add_test(tc_core, norm_dot_test);
	tcase_add_test(tc_core, cross_rotate_test);
	tcase_add_test(tc_core, plane_projection_test);
	suite_add_tcase(s, tc_core);

	return s;
}


int main(void)
{
	int number_failed;
	Suite *s = geometry_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_ENV);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

