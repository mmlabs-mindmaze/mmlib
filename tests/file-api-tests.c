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

#define NUM_FILE_CASE   MM_NELEM(init_setup_files)


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
	struct mm_timespec ts;
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
	struct mm_stat st;
	const struct file_info* info = &init_setup_files[_i];

	ck_assert(mm_stat(info->path, &st, 0) == 0);

	ck_assert_int_eq(st.size, info->sz);
	ck_assert_int_le(llabs(st.ctime - info->ctime), 1);
	ck_assert_int_eq(st.nlink, 1);
	ck_assert(S_ISREG(st.mode));
	ck_assert_int_eq((st.mode & 0777), info->mode);
}
END_TEST


START_TEST(fd_stat)
{
	int fd;
	struct mm_stat st;
	const struct file_info* info = &init_setup_files[_i];

	// Skip file that could not be open
	if (!(info->mode & S_IRUSR))
		return;

	fd = mm_open(info->path, O_RDONLY, 0);
	ck_assert(fd >= 0);

	ck_assert(mm_fstat(fd, &st) == 0);
	mm_close(fd);

	ck_assert_int_eq(st.size, info->sz);
	ck_assert_int_le(llabs(st.ctime - info->ctime), 1);
	ck_assert_int_eq(st.nlink, 1);
	ck_assert(S_ISREG(st.mode));
}
END_TEST


START_TEST(hard_link)
{
	struct mm_stat st, st_ln;
	const struct file_info* info = &init_setup_files[_i];

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
END_TEST

START_TEST(symbolic_link)
{
	char target[128];
	struct mm_stat st, st_ln;
	const struct file_info* info = &init_setup_files[_i];

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


START_TEST(copy_file)
{
	struct mm_stat st, st_cp;
	const struct file_info* info = &init_setup_files[_i];

	// Check number of link is initially 1
	ck_assert(mm_stat(info->path, &st, 0) == 0);
	ck_assert_int_eq(st.nlink, 1);

	// Skip test if file could not be open
	if (!(info->mode & S_IRUSR)) {
		ck_assert(mm_copy(info->path, TEST_FILE, 0, 0666));
		ck_assert(mm_get_lasterror_number() == EACCES);
		return;
	}

	// Make file copy
	ck_assert(mm_copy(info->path, TEST_FILE, 0, 0666) == 0);
	ck_assert(are_files_same(info->path, TEST_FILE) == true);

	ck_assert(mm_stat(info->path, &st, 0) == 0);
	ck_assert(mm_stat(TEST_FILE, &st_cp, 0) == 0);

	// Compare stat data (ino must be different)
	ck_assert_int_eq(st.dev, st_cp.dev);
	ck_assert(!mm_ino_equal(st.ino, st_cp.ino));
	ck_assert_int_eq(st_cp.nlink, 1);
	ck_assert_int_eq(st.nlink, 1);

	ck_assert(mm_unlink(TEST_FILE) == 0);
}
END_TEST


START_TEST(copy_symlink)
{
	struct mm_stat st, st_cp;
	const char* afile = init_setup_files[0].path;

	mm_symlink(afile, LINKNAME);

	// Make copy of symlink's target
	ck_assert(mm_copy(LINKNAME, TEST_FILE, 0, 0666) == 0);
	ck_assert(are_files_same(afile, TEST_FILE) == true);

	ck_assert(mm_stat(TEST_FILE, &st_cp, MM_NOFOLLOW) == 0);
	ck_assert(S_ISREG(st_cp.mode));
	ck_assert(mm_unlink(TEST_FILE) == 0);

	// Make symlink copy
	ck_assert(mm_copy(LINKNAME, TEST_FILE, MM_NOFOLLOW, 0666) == 0);
	ck_assert(are_files_same(afile, TEST_FILE) == true);

	ck_assert(mm_stat(LINKNAME, &st, MM_NOFOLLOW) == 0);
	ck_assert(mm_stat(TEST_FILE, &st_cp, MM_NOFOLLOW) == 0);

	ck_assert(S_ISLNK(st_cp.mode));
	ck_assert_int_eq(st.dev, st_cp.dev);
	ck_assert(!mm_ino_equal(st.ino, st_cp.ino));

	ck_assert(mm_unlink(TEST_FILE) == 0);

	ck_assert(mm_unlink(LINKNAME) == 0);
}
END_TEST


START_TEST(copy_fail)
{
	const char* afile = init_setup_files[0].path;
	mm_symlink(afile, LINKNAME);

	// Test not existing source fails with ENOENT
	ck_assert(mm_copy("does-not-exist", TEST_FILE, 0, 0666) == -1);
	ck_assert(mm_get_lasterror_number() == ENOENT);

	// test copy directory fails
	mm_mkdir("dir", 0777, O_CREAT);
	ck_assert(mm_copy("dir", TEST_FILE, 0, 0666) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EISDIR);
	mm_rmdir("dir");

	// test regular file copy does not overwrite file
	mm_copy(afile, TEST_FILE, 0, 0666);
	ck_assert(mm_copy(afile, TEST_FILE, 0, 0666) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EEXIST);
	mm_unlink(TEST_FILE);

	// test regular file copy does not overwrite file
	mm_copy(LINKNAME, TEST_FILE, 0, 0666);
	ck_assert(mm_copy(LINKNAME, TEST_FILE, MM_NOFOLLOW, 0666) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EEXIST);
	mm_unlink(TEST_FILE);

	mm_unlink(LINKNAME);
}
END_TEST


START_TEST(check_access_not_exist)
{
	ck_assert_int_eq(mm_check_access("does-not-exist", F_OK), ENOENT);
}
END_TEST


START_TEST(check_access_mode)
{
	int exp_rv;
	const struct file_info* info = &init_setup_files[_i];

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


START_TEST(rename_simple_file)
{
	int fd;

	fd = mm_open(TEST_FILE, O_CREAT, S_IWUSR|S_IRUSR);
	ck_assert(fd >= 0);
	mm_close(fd);

	// check that renaming works while the file is not used
	ck_assert(mm_rename(TEST_FILE, "new") == 0);

	fd = mm_open("new", O_RDWR, S_IWUSR|S_IRUSR);
	ck_assert(fd >= 0);

	// check that renaming works while the file is opened
	ck_assert(mm_rename("new", TEST_FILE) == 0);
	mm_close(fd);

	ck_assert(mm_check_access(TEST_FILE, F_OK) == 0);
}
END_TEST


START_TEST(rename_empty_directory)
{
	MM_DIR * dir;

	ck_assert(mm_mkdir("dir", 0777, O_CREAT) == 0);

	// check that renaming works while the directory is not used
	ck_assert(mm_rename("dir", "newdir") == 0);
	ck_assert(mm_check_access("newdir", F_OK) == 0);

	dir = mm_opendir("newdir");
	ck_assert(dir != NULL);

	// check that renaming works while the directory is opened
	ck_assert(mm_rename("newdir", "otherdir") == 0);
	ck_assert(mm_check_access("otherdir", F_OK) == 0);
	mm_closedir(dir);
	mm_rmdir("otherdir");
}
END_TEST


START_TEST(file_times)
{
	const char* afile = init_setup_files[0].path;
	const struct mm_timespec ts1 = {.tv_sec = 1234567890, .tv_nsec = 321};
	const struct mm_timespec ts2 = {.tv_sec = 1239999890, .tv_nsec = 111};
	const struct mm_timespec ts3 = {.tv_sec = 2239999890, .tv_nsec = 444};
	struct mm_timespec ts[2];
	struct mm_stat buf;

	ts[0] = ts1;
	ts[1] = ts2;
	ck_assert(mm_utimens(afile, ts, 0) == 0);
	mm_stat(afile, &buf, 0);
	ck_assert_int_eq(buf.atime, ts1.tv_sec);
	ck_assert_int_eq(buf.mtime, ts2.tv_sec);

	ts[0] = ts3;
	ts[1] = (struct mm_timespec) {.tv_nsec = UTIME_OMIT, .tv_sec = 1};
	ck_assert(mm_utimens(afile, ts, 0) == 0);
	mm_stat(afile, &buf, 0);
	ck_assert_int_eq(buf.atime, ts3.tv_sec);
	ck_assert_int_eq(buf.mtime, ts2.tv_sec);

	ts[0] = (struct mm_timespec) {.tv_nsec = UTIME_OMIT, .tv_sec = 1};
	ts[1] = ts1;
	ck_assert(mm_utimens(afile, ts, 0) == 0);
	mm_stat(afile, &buf, 0);
	ck_assert_int_eq(buf.atime, ts3.tv_sec);
	ck_assert_int_eq(buf.mtime, ts1.tv_sec);
}
END_TEST


START_TEST(file_times_now)
{
	const char* afile = init_setup_files[0].path;
	const struct mm_timespec ts1 = {.tv_sec = 1234567890, .tv_nsec = 321};
	const struct mm_timespec ts2 = {.tv_sec = 1239999890, .tv_nsec = 111};
	const struct mm_timespec ts3 = {.tv_sec = 2239999890, .tv_nsec = 444};
	struct mm_timespec ts[2];
	struct mm_stat buf;
	struct mm_timespec tmin, tmax;

	ts[0] = ts1;
	ts[1] = ts2;
	ck_assert(mm_utimens(afile, ts, 0) == 0);
	mm_stat(afile, &buf, 0);
	ck_assert_int_eq(buf.atime, ts1.tv_sec);
	ck_assert_int_eq(buf.mtime, ts2.tv_sec);

	ts[0] = ts3;
	ts[1] = (struct mm_timespec) {.tv_nsec = UTIME_NOW, .tv_sec = 1};
	mm_gettime(MM_CLK_REALTIME, &tmin);
	ck_assert(mm_utimens(afile, ts, 0) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmax);
	mm_stat(afile, &buf, 0);
	ck_assert_int_eq(buf.atime, ts3.tv_sec);
	ck_assert_int_ge(buf.mtime, tmin.tv_sec);
	ck_assert_int_le(buf.mtime, tmax.tv_sec);

	ts[0] = (struct mm_timespec) {.tv_nsec = UTIME_NOW, .tv_sec = 1};
	ts[1] = ts1;
	mm_gettime(MM_CLK_REALTIME, &tmin);
	ck_assert(mm_utimens(afile, ts, 0) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmax);
	mm_stat(afile, &buf, 0);
	ck_assert_int_ge(buf.atime, tmin.tv_sec);
	ck_assert_int_le(buf.atime, tmax.tv_sec);
	ck_assert_int_eq(buf.mtime, ts1.tv_sec);

	ts[0] = ts1;
	ts[1] = ts2;
	ck_assert(mm_utimens(afile, ts, 0) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmin);
	ck_assert(mm_utimens(afile, NULL, 0) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmax);
	mm_stat(afile, &buf, 0);
	ck_assert_int_ge(buf.atime, tmin.tv_sec);
	ck_assert_int_le(buf.atime, tmax.tv_sec);
	ck_assert_int_eq(buf.mtime, buf.atime);
}
END_TEST


START_TEST(file_fd_times)
{
	const struct mm_timespec ts1 = {.tv_sec = 1234567890, .tv_nsec = 321};
	const struct mm_timespec ts2 = {.tv_sec = 1239999890, .tv_nsec = 111};
	const struct mm_timespec ts3 = {.tv_sec = 2239999890, .tv_nsec = 444};
	struct mm_timespec ts[2];
	struct mm_stat buf;
	int fd = mm_open(init_setup_files[0].path, O_RDONLY, 0);

	ts[0] = ts1;
	ts[1] = ts2;
	ck_assert(mm_futimens(fd, ts) == 0);
	mm_fstat(fd, &buf);
	ck_assert_int_eq(buf.atime, ts1.tv_sec);
	ck_assert_int_eq(buf.mtime, ts2.tv_sec);

	ts[0] = ts3;
	ts[1] = (struct mm_timespec) {.tv_nsec = UTIME_OMIT, .tv_sec = 1};
	ck_assert(mm_futimens(fd, ts) == 0);
	mm_fstat(fd, &buf);
	ck_assert_int_eq(buf.atime, ts3.tv_sec);
	ck_assert_int_eq(buf.mtime, ts2.tv_sec);

	ts[0] = (struct mm_timespec) {.tv_nsec = UTIME_OMIT, .tv_sec = 1};
	ts[1] = ts1;
	ck_assert(mm_futimens(fd, ts) == 0);
	mm_fstat(fd, &buf);
	ck_assert_int_eq(buf.atime, ts3.tv_sec);
	ck_assert_int_eq(buf.mtime, ts1.tv_sec);

	mm_close(fd);
}
END_TEST


START_TEST(file_fd_times_now)
{
	const struct mm_timespec ts1 = {.tv_sec = 1234567890, .tv_nsec = 321};
	const struct mm_timespec ts2 = {.tv_sec = 1239999890, .tv_nsec = 111};
	const struct mm_timespec ts3 = {.tv_sec = 2239999890, .tv_nsec = 444};
	struct mm_timespec ts[2];
	struct mm_stat buf;
	struct mm_timespec tmin, tmax;
	int fd = mm_open(init_setup_files[0].path, O_RDONLY, 0);

	ts[0] = ts1;
	ts[1] = ts2;
	ck_assert(mm_futimens(fd, ts) == 0);
	mm_fstat(fd, &buf);
	ck_assert_int_eq(buf.atime, ts1.tv_sec);
	ck_assert_int_eq(buf.mtime, ts2.tv_sec);

	ts[0] = ts3;
	ts[1] = (struct mm_timespec) {.tv_nsec = UTIME_NOW, .tv_sec = 1};
	mm_gettime(MM_CLK_REALTIME, &tmin);
	ck_assert(mm_futimens(fd, ts) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmax);
	mm_fstat(fd, &buf);
	ck_assert_int_eq(buf.atime, ts3.tv_sec);
	ck_assert_int_ge(buf.mtime, tmin.tv_sec);
	ck_assert_int_le(buf.mtime, tmax.tv_sec);

	ts[0] = (struct mm_timespec) {.tv_nsec = UTIME_NOW, .tv_sec = 1};
	ts[1] = ts1;
	mm_gettime(MM_CLK_REALTIME, &tmin);
	ck_assert(mm_futimens(fd, ts) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmax);
	mm_fstat(fd, &buf);
	ck_assert_int_ge(buf.atime, tmin.tv_sec);
	ck_assert_int_le(buf.atime, tmax.tv_sec);
	ck_assert_int_eq(buf.mtime, ts1.tv_sec);

	ts[0] = ts1;
	ts[1] = ts2;
	ck_assert(mm_futimens(fd, ts) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmin);
	ck_assert(mm_futimens(fd, NULL) == 0);
	mm_gettime(MM_CLK_REALTIME, &tmax);
	mm_fstat(fd, &buf);
	ck_assert_int_ge(buf.atime, tmin.tv_sec);
	ck_assert_int_le(buf.atime, tmax.tv_sec);
	ck_assert_int_eq(buf.mtime, buf.atime);

	mm_close(fd);
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
	int flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);
	mm_chdir(BUILDDIR);
	mm_remove(TEST_DIR, MM_DT_ANY|MM_RECURSIVE);
	mm_error_set_flags(flags, MM_ERROR_IGNORE);
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

	tcase_add_loop_test(tc, path_stat, 0, NUM_FILE_CASE);
	tcase_add_loop_test(tc, fd_stat, 0, NUM_FILE_CASE);
	tcase_add_test(tc, check_access_not_exist);
	tcase_add_loop_test(tc, check_access_mode, 0, NUM_FILE_CASE);
	tcase_add_loop_test(tc, hard_link, 0, NUM_FILE_CASE);
	tcase_add_loop_test(tc, symbolic_link, 0, NUM_FILE_CASE);
	tcase_add_test(tc, dir_symbolic_link);
	tcase_add_test(tc, dangling_symlink);
	tcase_add_loop_test(tc, copy_file, 0, NUM_FILE_CASE);
	tcase_add_test(tc, copy_symlink);
	tcase_add_test(tc, copy_fail);
	tcase_add_test(tc, unlink_before_close);
	tcase_add_test(tc, one_way_pipe);
	tcase_add_test(tc, read_closed_pipe);
	tcase_add_test(tc, rename_simple_file);
	tcase_add_test(tc, rename_empty_directory);
	tcase_add_test(tc, file_times);
	tcase_add_test(tc, file_times_now);
	tcase_add_test(tc, file_fd_times);
	tcase_add_test(tc, file_fd_times_now);

	return tc;
}
