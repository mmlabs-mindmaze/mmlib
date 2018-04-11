/*
   @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>

#include "mmerrno.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "mmsysio.h"

#define TEST_SHM "test-shm-name"

static
void test_teardown(void)
{
	mm_shm_unlink(TEST_SHM);
}

START_TEST(shm_open_test)
{
	int fd;

	fd = mm_shm_open(TEST_SHM, O_CREAT, 0);
	ck_assert(fd > 0);
	ck_assert(mm_shm_unlink(TEST_SHM) == 0);
	mm_close(fd);
}
END_TEST

START_TEST(shm_open_duplicate_test)
{
	int fd1, fd2;

	fd1 = mm_shm_open(TEST_SHM, O_CREAT|O_EXCL, S_IWUSR);
	ck_assert(fd1 > 0);

	fd2 = mm_shm_open(TEST_SHM, O_CREAT|O_EXCL, S_IWUSR);
	ck_assert(fd2 == -1);

	ck_assert(mm_shm_unlink(TEST_SHM) == 0);
	mm_close(fd1);
}
END_TEST

START_TEST(mapfile_test)
{
	int fd;
	void * map;

	fd = mm_shm_open(TEST_SHM, O_RDWR|O_CREAT|O_EXCL, 0);
	ck_assert(fd > 0);

	mm_ftruncate(fd, MM_PAGESZ);
	map = mm_mapfile(fd, 0, MM_PAGESZ, MM_MAP_RDWR|MM_MAP_SHARED);
	ck_assert(map != NULL);

	mm_unmap(map);
	ck_assert(mm_shm_unlink(TEST_SHM) == 0);
	mm_close(fd);
}
END_TEST

START_TEST(mapfile_invalid_offset_test)
{
	int fd;
	void * map;

	fd = mm_shm_open(TEST_SHM, O_RDWR|O_CREAT|O_EXCL, 0);
	ck_assert(fd > 0);

	mm_ftruncate(fd, MM_PAGESZ);
	map = mm_mapfile(fd, 42, MM_PAGESZ, MM_MAP_EXEC|MM_MAP_SHARED);
	ck_assert(map == NULL);

	ck_assert(mm_shm_unlink(TEST_SHM) == 0);
	mm_close(fd);
}
END_TEST


START_TEST(invalid_unmap_test)
{
	ck_assert(mm_unmap((void * ) -1) == -1);
}
END_TEST

#define N 10000
START_TEST(multiple_maps_test)
{
	int i;
	int fds[N];
	void * maps[N];

	for (i = 0 ; i < N ; i++) {
		fds[i] = mm_anon_shm();
		if (fds[i] == -1) {
			ck_assert(mm_get_lasterror_number() == EMFILE);
			break;
		}

		mm_ftruncate(fds[i], MM_PAGESZ);
		maps[i] = mm_mapfile(fds[i], 0, MM_PAGESZ, MM_MAP_RDWR|MM_MAP_SHARED);
		if (maps[i] == NULL)
			break;
	}
	while (--i > 0) {
		ck_assert(fds[i] > 0);
		ck_assert(maps[i] != NULL);
		mm_close(fds[i]);
	}
}
END_TEST
#undef N

LOCAL_SYMBOL
TCase* create_shm_tcase(void)
{
	TCase * tc;
	tc = tcase_create("shm");
	tcase_add_checked_fixture(tc, NULL, test_teardown);

	tcase_add_test(tc, shm_open_test);
	tcase_add_test(tc, shm_open_duplicate_test);
	tcase_add_test(tc, mapfile_test);
	tcase_add_test(tc, mapfile_invalid_offset_test);
	tcase_add_test(tc, invalid_unmap_test);
	tcase_add_test(tc, multiple_maps_test);

	return tc;
}
