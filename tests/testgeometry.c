/*
   @mindmaze_header@
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

static const float refquat[][4] = {
	[0] = {0,       0.447,  0.447,   0.775},
	[1] = {0.707,   0.316,  0.316,   0.548},
	[2] = {0,       0,      1,       0},
	[3] = {1,       0,      0,       0},
	[4] = {0.70711, 0,     -0.70711, 0},
};

static const float refmat[][9] = {
	[0] = {-0.600867987,  0.399618,     0.692849994,
	        0.399618,    -0.600867987,  0.692849994,
	        0.692849994,  0.692849994,  0.200764
	},
	[1] = { 0.19968003,  -0.575159967,  0.793160081,
	        0.974584043,  0.19968003,  -0.100488037,
	       -0.100488037,  0.793160081,  0.600575924
	},
	[2] = {-1,            0,            0,
	        0,            1,            0,
	        0,            0,           -1
	},
	[3] = { 1,            0,            0,
	        0,            1,            0,
	        0,            0,            1
	},
	[4] = { 0,            0,           -1,
	        0,            1,            0,
	        1,            0,            0
	},

};

// in aaxis, the first 3 components are axis, the 4th is angle in radians
static const float refaaxis[][4] = {
	[0] = { 0.44688,  0.44688,  0.77498,  3.1416},
	[1] = { 0.44688,  0.44688,  0.77498,  1.5710},
	[2] = { 0,        1,        0,        3.1416},
	[3] = { 1,        0,        0,        0},
	[4] = { 0,       -1,        0,        1.5708},
};

#define NUM_REF	(sizeof(refquat)/sizeof(refquat[0]))

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


START_TEST(aaxis_from_mat3_test)
{
	float aaxis[4];
	aaxis[3] = mm_aaxis_from_mat3(aaxis, refmat[_i]);
	fail_if(!is_equal(aaxis, refaaxis[_i], 4), "iter %i failed", _i);
}
END_TEST


START_TEST(mat_from_quat_test)
{
	float mat[9];
	mm_mat3_from_quat(mat, refquat[_i]);
	fail_if(!is_equal(mat, refmat[_i], 9), "iteration %i failed", _i);
}
END_TEST


START_TEST(quat_from_mat_test)
{
	float quat[4];
	mm_quat_from_mat3(quat, refmat[_i]);
	fail_if(!is_equal(quat, refquat[_i], 4), "iteration %i failed", _i);
}
END_TEST


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
	tcase_add_loop_test(tc_core, mat_from_quat_test, 0, NUM_REF);
	tcase_add_loop_test(tc_core, quat_from_mat_test, 0, NUM_REF);
	tcase_add_loop_test(tc_core, aaxis_from_mat3_test, 0, NUM_REF);
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

