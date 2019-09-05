/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "mmsysio.h"
#include "mmerrno.h"
#include "mmtime.h"
#include "api-testcases.h"

#define TEST_DATA       "string for data test"
#define TEST_DIR        BUILDDIR"/file-testdir"
#define LINKNAME        "newlink"
#define TEST_FILE       "test.dat"

struct file_info {
	char path[64];
	int mode;
	size_t sz;
	time_t ctime;
};

static struct file_info init_setup_files[] = {
	{.path = "a_filename",  .mode = S_IRUSR,          .sz = 980650},
	{.path = u8"gggg5ψ",    .mode = S_IRUSR,          .sz = 980650},
	{.path = u8"f2éà",      .mode = S_IWUSR,          .sz = 541},
	{.path = u8"afile㐘",   .mode = S_IWUSR|S_IRUSR,  .sz = 1024},
	{.path = u8"dfg-gf",    .mode = S_IRWXU,          .sz = 8192},
	{.path = u8"ozuç㐘rye", .mode = S_IRUSR|S_IXUSR,  .sz = 6980},
	{.path = u8"fdgrye",    .mode = 0,                .sz = 1065000},
};


static
void gen_rand_data(char* buff, size_t len)
{
	int data;

	while (len > 0) {
		data = rand();

		if (len < sizeof(data)) {
			memcpy(buff, &data, len);
			break;
		}

		*(int*)buff = data;
		len -= sizeof(data);
		buff += sizeof(data);
	}
}


static
int create_file(const char* path, int mode, size_t filelen)
{
	int fd, rv;
	char buff[512];
	ssize_t rsz;
	size_t len;

	fd = mm_open(path, O_CREAT|O_WRONLY, mode);
	if (fd < 0)
		return -1;

	rv = 0;
	while (filelen) {
		len = filelen > sizeof(buff) ? sizeof(buff) : filelen;

		gen_rand_data(buff, len);

		rsz = mm_write(fd, buff, len);
		if (rsz <= 0) {
			rv = -1;
			break;
		}

		filelen -= rsz;
	}

	mm_close(fd);
	return rv;
}


static
int create_entry(struct file_info* info)
{
	struct timespec ts;
	char suffix[16];

	// Append a random suffix to prevent file metadata reuse on windows
	mm_gettime(MM_CLK_REALTIME, &ts);
	sprintf(suffix, "-%li", (long)ts.tv_nsec);
	strcat(info->path, suffix);

	time(&info->ctime);
	if (create_file(info->path, info->mode, info->sz))
		return -1;

	return 0;
}


static
int fullread(int fd, void* buf, size_t len)
{
	char* cbuf = buf;
	ssize_t rsz;

	while (len) {
		rsz = mm_read(fd, cbuf, len);
		if (rsz <= 0)
			return -1;

		len -= rsz;
		cbuf += rsz;
	}

	return 0;
}


static
bool are_files_same(const char* path1, const char* path2)
{
	int fd1, fd2;
	ssize_t rsz;
	char buff1[512], buff2[512];
	bool result = false;

	fd1 = mm_open(path1, O_RDONLY, 0);
	fd2 = mm_open(path2, O_RDONLY, 0);
	if (fd1 < 0 || fd2 < 0)
		goto exit;

	while (1) {
		rsz = mm_read(fd1, buff1, sizeof(buff1));
		if (rsz < 0)
			goto exit;

		// Check end of file1 has been reached
		if (rsz == 0) {
			// Check file2 is also at end of file
			rsz = mm_read(fd2, buff2, sizeof(buff2));
			result = (rsz == 0) ? true : false;
			break;
		}

		// Read from file2 and compare data
		if (fullread(fd2, buff2, rsz)
		   || memcmp(buff1, buff2, rsz) != 0  )
			break;
	}

exit:
	mm_close(fd1);
	mm_close(fd2);

	return result;
}


START_TEST(path_stat)
{
	int i;
	struct mm_stat st;
	const struct file_info* info;

	for (i = 0; i < MM_NELEM(init_setup_files); i++) {
		info = &init_setup_files[i];

		ck_assert(mm_stat(info->path, &st, 0) == 0);

		ck_assert_int_eq(st.size, info->sz);
		ck_assert_int_le(abs(st.ctime - info->ctime), 1);
		ck_assert_int_eq(st.nlink, 1);
		ck_assert(S_ISREG(st.mode));
		ck_assert_int_eq((st.mode & 0777), info->mode);
	}
}
END_TEST


START_TEST(fd_stat)
{
	int i, fd;
	struct mm_stat st;
	const struct file_info* info;

	for (i = 0; i < MM_NELEM(init_setup_files); i++) {
		info = &init_setup_files[i];

		// Skip file that could not be open
		if (!(info->mode & S_IRUSR))
			continue;

		fd = mm_open(info->path, O_RDONLY, 0);
		ck_assert(fd >= 0);

		ck_assert(mm_fstat(fd, &st) == 0);
		mm_close(fd);

		ck_assert_int_eq(st.size, info->sz);
		ck_assert_int_le(abs(st.ctime - info->ctime), 1);
		ck_assert_int_eq(st.nlink, 1);
		ck_assert(S_ISREG(st.mode));
	}
}
END_TEST


START_TEST(hard_link)
{
	int i;
	struct mm_stat st, st_ln;
	const struct file_info* info;

	for (i = 0; i < MM_NELEM(init_setup_files); i++) {
		info = &init_setup_files[i];

		// Check number of link is initially 1
		ck_assert(mm_stat(info->path, &st, 0) == 0);
		ck_assert_int_eq(st.nlink, 1);

		// Make link and check content is identical
		ck_assert(mm_link(info->path, LINKNAME) == 0);

		// Skip test if file could not be open
		if (info->mode & S_IRUSR)
			ck_assert(are_files_same(info->path, LINKNAME) == true);

		ck_assert(mm_stat(info->path, &st, 0) == 0);
		ck_assert(mm_stat(LINKNAME, &st_ln, 0) == 0);

		ck_assert_int_eq(st.nlink, 2);

		// Compate stat data (must be identical)
		ck_assert_int_eq(st.dev, st_ln.dev);
		ck_assert(mm_ino_equal(st.ino, st_ln.ino));
		ck_assert_int_eq(st.size, st_ln.size);
		ck_assert_int_eq(st.ctime, st_ln.ctime);
		ck_assert_int_eq(st.nlink, st_ln.nlink);
		ck_assert_int_eq(st.mode, st_ln.mode);

		ck_assert(mm_unlink(LINKNAME) == 0);
		ck_assert(mm_stat(info->path, &st, 0) == 0);
		ck_assert_int_eq(st.nlink, 1);
	}
}
END_TEST

START_TEST(symbolic_link)
{
	int i;
	char target[128];
	struct mm_stat st, st_ln;
	const struct file_info* info;

	for (i = 0; i < MM_NELEM(init_setup_files); i++) {
		info = &init_setup_files[i];

		// Make link and check content is identical
		ck_assert(mm_symlink(info->path, LINKNAME) == 0);

		// Skip test if file could not be open
		if (info->mode & S_IRUSR)
			ck_assert(are_files_same(info->path, LINKNAME) == true);

		ck_assert(mm_stat(LINKNAME, &st_ln, MM_NOFOLLOW) == 0);
		ck_assert(S_ISLNK(st_ln.mode));
		ck_assert_int_eq(st_ln.size, strlen(info->path)+1);

		// Check that value of symlink is the one expected
		ck_assert(mm_readlink(LINKNAME, target, sizeof(target)) == 0);
		ck_assert(strcmp(target, info->path) == 0);

		// Check behavior of mm_readlink() if buffer too small
		ck_assert(mm_readlink(LINKNAME, target, 3) == -1);
		ck_assert_int_eq(mm_get_lasterror_number(), EOVERFLOW);

		// Check that symlink do not increase link number of
		// original file
		ck_assert(mm_stat(info->path, &st, 0) == 0);
		ck_assert_int_eq(st.nlink, 1);

		// Check that symlink and original file are indeed
		// different files on filesystem
		ck_assert(!mm_ino_equal(st.ino, st_ln.ino));

		// Compare stat data of pointed file (must be identical)
		ck_assert(mm_stat(LINKNAME, &st_ln, 0) == 0);
		ck_assert_int_eq(st.dev, st_ln.dev);
		ck_assert(mm_ino_equal(st.ino, st_ln.ino));
		ck_assert_int_eq(st.size, st_ln.size);
		ck_assert_int_eq(st.ctime, st_ln.ctime);
		ck_assert_int_eq(st.nlink, st_ln.nlink);
		ck_assert_int_eq(st.mode, st_ln.mode);

		ck_assert(mm_unlink(LINKNAME) == 0);
		ck_assert(mm_stat(info->path, &st, 0) == 0);
		ck_assert_int_eq(st.nlink, 1);
	}
}
END_TEST


START_TEST(dir_symbolic_link)
{
	int fd;
	struct mm_stat st = {0};
	struct mm_stat st_ref = {0};

	mm_mkdir("somedir", 0777, 0);

	// Create symlink to somefile parent dir
	ck_assert(mm_symlink("somedir", "link-to-somedir") == 0);

	ck_assert(mm_stat("link-to-somedir", &st, MM_NOFOLLOW) == 0);
	ck_assert(S_ISLNK(st.mode));
	ck_assert(mm_stat("link-to-somedir", &st, 0) == 0);
	ck_assert(S_ISDIR(st.mode));

	// Create file in somedir
	fd = mm_open("somedir/somefile", O_CREAT|O_WRONLY, 0666);
	mm_close(fd);
	mm_stat("somedir/somefile", &st_ref, 0);

	// Check file opened with symlinked parent dir point to same file
	fd = mm_open("link-to-somedir/somefile", O_RDONLY, 0);
	ck_assert(mm_fstat(fd, &st) == 0);
	ck_assert(mm_ino_equal(st.ino, st_ref.ino));
	ck_assert_int_eq(st.dev, st_ref.dev);
	mm_close(fd);

	ck_assert(mm_unlink("link-to-somedir") == 0);
	mm_rmdir("somedir");
}
END_TEST


// On Windows following a dangling directory symlink returns EACCES error
// while on POSIX platforms it returns ENOENT. Since the cost to harmonize
// this is rather high while the inconveniency is rather small, we leave it
// as it is
#if defined(WIN32)
#  define DANGLING_DIR_SYM_ERROR EACCES
#else
#  define DANGLING_DIR_SYM_ERROR ENOENT
#endif


static
void assert_dangling_symlink(const char* linkname, int exp_err)
{
	ck_assert(mm_open(linkname, O_RDONLY, 0) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), exp_err);
}


START_TEST(dangling_symlink)
{
	int fd;

	// Create symlink on non existing target
	ck_assert(mm_symlink("./does-not-exist", LINKNAME) == 0);
	assert_dangling_symlink(LINKNAME, ENOENT);
	assert_dangling_symlink(LINKNAME"/afile", ENOENT);
	ck_assert(mm_unlink(LINKNAME) == 0);

	// Create symlink to disappearing target dir
	ck_assert(mm_mkdir("adir", 0777, 0) == 0);
	ck_assert(mm_symlink("adir", LINKNAME) == 0);
	ck_assert(mm_remove("adir", MM_DT_ANY) == 0);
	assert_dangling_symlink(LINKNAME, DANGLING_DIR_SYM_ERROR);
	assert_dangling_symlink(LINKNAME"/afile", ENOENT);
	ck_assert(mm_unlink(LINKNAME) == 0);

	// Create symlink to disappearing target file
	fd = mm_open("afile", O_CREAT|O_WRONLY, 0666);
	mm_close(fd);
	ck_assert(mm_symlink("afile", LINKNAME) == 0);
	mm_remove("afile", MM_DT_ANY);
	assert_dangling_symlink(LINKNAME, ENOENT);
	ck_assert(mm_unlink(LINKNAME) == 0);
}
END_TEST


START_TEST(check_access)
{
	int i, exp_rv;
	const struct file_info* info;

	ck_assert_int_eq(mm_check_access("does-not-exist", F_OK), ENOENT);

	for (i = 0; i < MM_NELEM(init_setup_files); i++) {
		info = &init_setup_files[i];

		// File must exist
		ck_assert_int_eq(mm_check_access(info->path, F_OK), 0);

		// Test read access
		exp_rv = (info->mode & S_IRUSR) ? 0 : EACCES;
		ck_assert_int_eq(mm_check_access(info->path, R_OK), exp_rv);

		// Test write access
		exp_rv = (info->mode & S_IWUSR) ? 0 : EACCES;
		ck_assert_int_eq(mm_check_access(info->path, W_OK), exp_rv);

		// Test execute access
		exp_rv = (info->mode & S_IXUSR) ? 0 : EACCES;
		ck_assert_int_eq(mm_check_access(info->path, X_OK), exp_rv);
	}
}
END_TEST


START_TEST(unlink_before_close)
{
	int fd, fd2;
	char str[] = "Hello world!";
	char buf[64];

	fd = mm_open(TEST_FILE, O_CREAT|O_TRUNC|O_WRONLY, S_IWUSR|S_IRUSR);
	ck_assert(fd >= 0);
	fd2 = mm_open(TEST_FILE, O_RDONLY, 0);
	ck_assert(fd2 >= 0);

	// Remove file for file system
	ck_assert(mm_unlink(TEST_FILE) == 0);
	ck_assert_int_eq(mm_check_access(TEST_FILE, F_OK), ENOENT);

	// Write to one fd
	ck_assert_int_eq(mm_write(fd, str, sizeof(str)), sizeof(str));
	ck_assert(mm_close(fd) == 0);

	// Read from other fd
	ck_assert_int_eq(mm_read(fd2, buf, sizeof(str)), sizeof(str));
	ck_assert(memcmp(buf, str, sizeof(str)) == 0);

	ck_assert(mm_close(fd2) == 0);
}
END_TEST


START_TEST(one_way_pipe)
{
	int fds[2];
	int i;
	ssize_t rsz;
	size_t data_sz;
	char* data;
	struct mm_error_state errstate;
	char buff[sizeof(TEST_DATA)+42];

	ck_assert(mm_pipe(fds) == 0);

	// Write to proper endpoint and read from the other and check data
	// is correct
	for (i = 0; i < 10; i++) {
		data = TEST_DATA + i;
		data_sz = sizeof(TEST_DATA) - i;

		rsz = mm_write(fds[1], data, data_sz);
		ck_assert_int_eq(rsz, data_sz);

		rsz = mm_read(fds[0], buff, sizeof(buff));
		ck_assert_int_eq(rsz, data_sz);

		ck_assert(memcmp(buff, data, data_sz) == 0);
	}

	mm_save_errorstate(&errstate);

	// Check writing is rejected on read endpoint and reading is rejected
	// on write endpoint
	ck_assert(mm_write(fds[0], TEST_DATA, sizeof(TEST_DATA)) == -1);
	ck_assert(mm_read(fds[1], TEST_DATA, sizeof(TEST_DATA)) == -1);

	mm_set_errorstate(&errstate);

	ck_assert(mm_close(fds[0]) == 0);
	ck_assert(mm_close(fds[1]) == 0);

}
END_TEST


START_TEST(read_closed_pipe)
{
	int fds[2];
	ssize_t rsz, wsz;
	char buff[128];

	gen_rand_data(buff, sizeof(buff));

	// Create connected pipe endpoints
	ck_assert(mm_pipe(fds) == 0);

	// Write random and close the write end
	rsz = mm_write(fds[1], buff, sizeof(buff)/2);
	ck_assert(rsz == sizeof(buff)/2);
	mm_close(fds[1]);

	// Read bigger size, ensure that data is cropped to what was written
	wsz = mm_read(fds[0], buff, sizeof(buff));
	ck_assert(rsz == wsz);

	// At this point, the pipe is empty
	// Ensure that all read read return 0
	ck_assert(mm_read(fds[0], buff, sizeof(buff)) == 0);
	ck_assert(mm_read(fds[0], buff, sizeof(buff)) == 0);
	ck_assert(mm_read(fds[0], buff, sizeof(buff)) == 0);

	mm_close(fds[0]);

}
END_TEST


/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/


static
void cleanup_testdir(void)
{
	mm_chdir(BUILDDIR);
	mm_remove(TEST_DIR, MM_DT_ANY|MM_RECURSIVE);
}


static
void init_testdir(void)
{
	int i;

	mm_remove(TEST_DIR, MM_DT_ANY|MM_RECURSIVE);

	if (mm_mkdir(TEST_DIR, S_IRWXU, MM_RECURSIVE)
	   || mm_chdir(TEST_DIR)  )
		return;

	for (i = 0; i < MM_NELEM(init_setup_files); i++) {
		if (create_entry(&init_setup_files[i]))
			return;
	}
}

LOCAL_SYMBOL
TCase* create_file_tcase(void)
{
	TCase *tc = tcase_create("file");

	tcase_add_unchecked_fixture(tc, init_testdir, cleanup_testdir);

	tcase_add_test(tc, path_stat);
	tcase_add_test(tc, fd_stat);
	tcase_add_test(tc, check_access);
	tcase_add_test(tc, hard_link);
	tcase_add_test(tc, symbolic_link);
	tcase_add_test(tc, dir_symbolic_link);
	tcase_add_test(tc, dangling_symlink);
	tcase_add_test(tc, unlink_before_close);
	tcase_add_test(tc, one_way_pipe);
	tcase_add_test(tc, read_closed_pipe);

	return tc;
}
