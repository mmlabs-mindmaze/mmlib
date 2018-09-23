/*
   @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "mmerrno.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "mmsysio.h"

#define TMP_DIR_ROOT BUILDDIR "/mmlib-test-dir"
#define TEST_DIR  "testdir"
#define TEST_FILE "testfile.dat"
#define TEST_LINK "testfile.lnk"


static void test_teardown(void)
{
	mm_chdir(BUILDDIR);
	mm_remove(TMP_DIR_ROOT, MM_DT_ANY|MM_RECURSIVE);
}

static void test_init(void)
{
	mm_mkdir(TMP_DIR_ROOT, 0777, 0); /* ignore error if already present */
	mm_chdir(TMP_DIR_ROOT);
}

START_TEST(test_dir_open_close)
{
	int rv;
	MMDIR * d;

	rv = mm_mkdir("1/2/3", 0777, MM_RECURSIVE);
	ck_assert(rv == 0);

	d = mm_opendir("1/2");
	ck_assert(d != NULL);

	mm_closedir(d);

}
END_TEST

START_TEST(test_dir_create_twice)
{
	ck_assert(mm_mkdir(TEST_DIR, 0777, 0) == 0);
	ck_assert(mm_check_access(TEST_DIR, F_OK) == 0);

	ck_assert(mm_mkdir(TEST_DIR, 0777, 0) != 0);
	ck_assert(mm_get_lasterror_number() == EEXIST);
	ck_assert(mm_mkdir(TEST_DIR, 0777, MM_RECURSIVE) == 0);
	ck_assert(mm_remove(TEST_DIR, MM_DT_DIR) == 0);
}
END_TEST



START_TEST(test_dir_create_rec)
{
	int rv;
	rv = mm_mkdir(
		"1/dir2/dir_3/dir 4/dir_with_a_longer_name_than_the_others",
		0777, MM_RECURSIVE);
	ck_assert(rv == 0);

	ck_assert(mm_chdir("does_not_exist") != 0);
	ck_assert(mm_chdir("1/dir2/dir_3/dir 4") == 0);
	ck_assert(mm_chdir("dir_with_a_longer_name_than_the_others") == 0);
}
END_TEST

static
char const* filtype_str(char const type)
{
	switch (type) {
	case MM_DT_FIFO: return "fifo";
	case MM_DT_CHR:  return "chr";
	case MM_DT_BLK:  return "blk";
	case MM_DT_DIR:  return "dir";
	case MM_DT_REG:  return "reg";
	case MM_DT_LNK:  return "link";
	default: return "unknown";
	}
}

START_TEST(test_dir_read)
{
	MMDIR * dir;
	struct mm_dirent const * dp;
	int fd;
	bool found_file, found_dir;
	int status;

	mm_mkdir("folder", 0777, 0);
	fd = mm_open(TEST_FILE, O_CREAT|O_TRUNC|O_RDWR, S_IWUSR|S_IRUSR);
	ck_assert(fd);

	ck_assert((dir = mm_opendir(".")) != NULL);
	found_file = found_dir = false;
	while ((dp = mm_readdir(dir, &status)) != NULL) {
		ck_assert(status == 0);
		/* should see the file and the folder we just created */
		found_dir |= (dp->type == MM_DT_DIR);
		found_file |= (dp->type == MM_DT_REG);
	}

	while ((dp = mm_readdir(dir, &status)) != NULL) {
		ck_assert(status == 0);
		ck_abort_msg(
			"readdir should not find anything without rewind()");
	}

	mm_closedir(dir);
}
END_TEST

START_TEST(test_remove)
{
	int fd, rv;

	fd = mm_open(TEST_FILE, O_CREAT|O_TRUNC|O_RDWR, S_IWUSR|S_IRUSR);
	ck_assert(fd);
	mm_close(fd);

	rv = mm_remove(TEST_FILE, MM_DT_ANY);
	ck_assert(rv == 0);

	fd = mm_open(TEST_FILE, O_RDWR, S_IWUSR|S_IRUSR);
	ck_assert(fd == -1);
}
END_TEST

START_TEST(test_remove_dir)
{
	ck_assert(mm_mkdir(TEST_DIR, 0777, 0) == 0);
	ck_assert(mm_remove(TEST_DIR, MM_DT_ANY) == 0);
	ck_assert(mm_check_access(TEST_DIR, F_OK) == ENOENT);

	/* same with the recursive flag */
	ck_assert(mm_mkdir(TEST_DIR, 0777, 0) == 0);
	ck_assert(mm_remove(TEST_DIR, MM_DT_ANY|MM_RECURSIVE) == 0);
	ck_assert(mm_check_access(TEST_DIR, F_OK) == ENOENT);
}
END_TEST


START_TEST(test_remove_type)
{
	int fd, rv;

	fd = mm_open(TEST_FILE, O_CREAT|O_TRUNC|O_RDWR, S_IWUSR|S_IRUSR);
	ck_assert(fd);
	mm_close(fd);

	/* should not remove anything, and fail with permission error:
	 * TEST_FILE is not a link*/
	rv = mm_remove(TEST_FILE, MM_DT_DIR);
	ck_assert(rv != 0);

	fd = mm_open(TEST_FILE, O_RDWR, S_IWUSR|S_IRUSR);
	ck_assert(fd != -1);
	mm_close(fd);
}
END_TEST

static
void realloc_if_needed(char ** buf, size_t *buf_len, size_t required_len)
{
	char * ptr;
	size_t new_len;

	if (required_len <= *buf_len)
		return;

	new_len = *buf_len;
	while (new_len < required_len)
		new_len *= 2;

	ptr = realloc(*buf, new_len);
	ck_assert(ptr);

	*buf = ptr;
	*buf_len = new_len;
}

/* return a buffer that should be free'd */
static
char* tree_rec(const char * path, int lvl, char ** buffer, size_t * buffer_len)
{
	int i, rv, status;
	MMDIR * dir;
	struct mm_dirent const * dp;
	char tmp[256];
	size_t len;

	ck_assert((dir = mm_opendir(path)) != NULL);
	while ((dp = mm_readdir(dir, &status)) != NULL) {
		ck_assert(status == 0);
		len = strlen(dp->name);
		if (  (len == 1 && memcmp(dp->name, ".", sizeof(".") - 1) == 0)
		   || (len == 2 && memcmp(dp->name, "..", sizeof("..") - 1) ==
		       0))
			continue;

		tmp[0] = '\0';

		for (i = 0; i < lvl; i++)
			strcat(tmp, "\t");

		strcat(tmp, "─");

		rv = sprintf(tmp + lvl, "├─ %s (%s)\n", dp->name,
		             filtype_str( dp->type));
		realloc_if_needed(buffer, buffer_len, strlen(*buffer) + lvl +
		                  rv);
		strcat(*buffer, tmp);

		if (dp->type == MM_DT_DIR) {
			char *new_path;

			new_path = malloc(strlen(path) + strlen(dp->name) + 2);
			*new_path = '\0';
			ck_assert(new_path != NULL);
			strcat(new_path, path);
			strcat(new_path, "/");
			strcat(new_path, dp->name);
			*buffer = tree_rec(new_path, lvl + 1, buffer,
			                   buffer_len);
			free(new_path);
		}
	}

	mm_closedir(dir);

	return *buffer;
}
static
char* tree(const char * path)
{
	size_t buffer_len = 256;
	char * buffer = malloc(buffer_len);
	ck_assert(buffer != NULL);
	buffer[0] = 0;
	buffer = tree_rec(path, 0, &buffer, &buffer_len);
	return buffer;
}


/*
 * Note about the order dirent() returns:
 * * Windows display files in alphabetical order
 * * Linux MOSTLY display files in time order
 *
 * Linux actually returns the entries depending on their order creation ...
 * AND depending on the inode recycling at the moment of the folder creation.
 * Most of the time it will seem like it's chronological, but it might not be
 */
#define TREE_REF_ALPHA\
	"├─ 1 (dir)\n\t├─ 2 (dir)\n\t\t├─ 3 (dir)\n\t\t\t├─ file1.dat (reg)\n├─ 4 (dir)\n\t├─ 5 (dir)\n\t\t├─ 6 (dir)\n\t\t\t├─ file1.lnk (link)\n"
#define TREE_REF_INODE\
	"├─ 4 (dir)\n\t├─ 5 (dir)\n\t\t├─ 6 (dir)\n\t\t\t├─ file1.lnk (link)\n├─ 1 (dir)\n\t├─ 2 (dir)\n\t\t├─ 3 (dir)\n\t\t\t├─ file1.dat (reg)\n"
#define TREE_1\
	"├─ 4 (dir)\n\t├─ 5 (dir)\n\t\t├─ 6 (dir)\n\t\t\t├─ file1.lnk (link)\n"
START_TEST(test_remove_rec)
{
	int file1_fd;
	ssize_t rsz;
	char * tree_ref, * tree1, * tree2;

	/*
	 * Create the following tree:
	 * (leave file2.dat open during the call to mm_remove)
	 * .
	 * ├── 1
	 * │   └── 2
	 * │       └── 3
	 * │           └── file1.dat
	 * ├── 4
	 * │   └── 5
	 * │       └── 6
	 * │           └── file1.lnk -> ../../../1/2/3/file1.dat
	 */
	ck_assert(mm_mkdir("1/2/3/", 0777, MM_RECURSIVE) == 0);
	ck_assert(mm_mkdir("4/5/6/", 0777, MM_RECURSIVE) == 0);

	ck_assert(mm_chdir("1/2/3/") == 0);
	file1_fd = mm_open("file1.dat", O_CREAT|O_TRUNC|O_RDWR, S_IWUSR|
	                   S_IRUSR);
	ck_assert(file1_fd > 0);
	/* do not close file1_fd */
	ck_assert(mm_chdir("../../../") == 0);
	ck_assert(mm_symlink("1/2/3/file1.dat", "4/5/6/file1.lnk") == 0);

	/* tests steps:
	 * - record initial tree
	 * - 1
	 *   - recursive remove all files
	 *   - check 1/2/3/file1.dat file and folders removal
	 *   - check 4/5/6/file1.lnk remains
	 *   - check file1.dat still open and usable
	 * - 2
	 *   - check file cannot be reopened
	 *   - close file1.dat
	 *   - recursive remove all files
	 *   - check tree unchanged
	 */
	tree_ref = tree(".");
	printf("Initial tree:\n%s", tree_ref);
	ck_assert(strcmp(tree_ref, TREE_REF_ALPHA) == 0
			|| strcmp(tree_ref, TREE_REF_INODE) == 0);

	/* - 1
	 *   - recursive remove all files
	 *   - check 1/2/3/file1.dat file and folders removal
	 *   - check 4/5/6/file1.lnk remains
	 *   - check file1.dat still open and usable */
	ck_assert(mm_remove(".", MM_DT_REG|MM_DT_DIR|MM_RECURSIVE) == 0);
	tree1 = tree(".");
	printf("Tree1:\n%s", tree1);
	ck_assert(strcmp(tree1, TREE_1) == 0);
	rsz = mm_write(file1_fd, "Hello world!", sizeof("Hello world!"));
	ck_assert(rsz == sizeof("Hello world!"));

	/* - 2
	 *   - check file cannot be reopened
	 *   - close file1.dat
	 *   - recursive remove all files
	 *   - check tree unchanged */
	ck_assert(mm_open("file1.dat", O_TRUNC|O_RDWR, S_IWUSR|S_IRUSR) == -1);
	mm_close(file1_fd);
	ck_assert(mm_remove(".", MM_DT_REG|MM_DT_DIR|MM_RECURSIVE) == 0);
	tree2 = tree(".");
	printf("Tree2:\n%s", tree2);
	ck_assert(strcmp(tree1, tree2) == 0);

	/* cleaning */
	free(tree2);
	free(tree1);
	free(tree_ref);
}
END_TEST


static
char* get_realpath(const char *path)
{
#if defined(_WIN32)
	return _fullpath(NULL, path, 0);
#else
	return realpath(path, NULL);
#endif
}


static
int are_paths_equal(const char* p1, const char* p2)
{
	int res;
	char *full_p1, *full_p2;

	full_p1 = get_realpath(p1);
	full_p2 = get_realpath(p2);

	res = (strcmp(full_p1, full_p2) == 0);

	free(full_p1);
	free(full_p2);

	return res;
}


START_TEST(get_currdir_wbuffer)
{
	char buf[sizeof(TMP_DIR_ROOT)];
	char* path;

	path = mm_getcwd(buf, sizeof(buf));
	ck_assert(path != NULL);
	if (!are_paths_equal(path, TMP_DIR_ROOT))
		ck_abort_msg("path mismatch: %s != %s", path, TMP_DIR_ROOT);
}
END_TEST


START_TEST(get_currdir_tooshort)
{
	char buf[sizeof(TMP_DIR_ROOT)-3];

	ck_assert(mm_getcwd(buf, sizeof(buf)) == NULL);
	ck_assert_int_eq(mm_get_lasterror_number(), ERANGE);
}
END_TEST


START_TEST(get_currdir_malloc)
{
	char* path;

	path = mm_getcwd(NULL, 0);
	ck_assert(path != NULL);
	if (!are_paths_equal(path, TMP_DIR_ROOT))
		ck_abort_msg("path mismatch: %s != %s", path, TMP_DIR_ROOT);

	free(path);
}
END_TEST

static
bool test_symlinks(void)
{
	int fd;
	bool has_symlink_support = false;

	mm_remove("symlink-test-file", MM_DT_ANY);
	mm_remove("symlink-test-file.lnk", MM_DT_ANY);

	fd = mm_open("symlink-test-file", O_CREAT|O_TRUNC|O_RDWR, S_IWUSR|
	             S_IRUSR);
	if (fd != -1) {
		has_symlink_support = (mm_symlink("symlink-test-file",
		                                  "symlink-test-file.lnk") ==
		                       0);
		mm_close(fd);
	}

	mm_remove("symlink-test-file", MM_DT_ANY);
	mm_remove("symlink-test-file.lnk", MM_DT_ANY);

	if (  !has_symlink_support
	   && strcmp(mm_getenv("TC_DIR_SYMLINK", "no"), "yes") != 0) {
		fprintf(stderr, "Skipping symlink tests: "
		                "unsupported on windows without special privileges\n"
		                "\n"
		                "Try runinng as administrator or with developper features enabled\n");
		return false;
	}

	return true;
}

LOCAL_SYMBOL
TCase* create_dir_tcase(void)
{
	TCase * tc;
	bool has_unprivileged_symlinks = test_symlinks();

	tc = tcase_create("dir");
	tcase_add_checked_fixture(tc, test_init, test_teardown);

	tcase_add_test(tc, test_dir_open_close);
	tcase_add_test(tc, test_dir_create_twice);
	tcase_add_test(tc, test_dir_create_rec);
	tcase_add_test(tc, test_dir_read);
	tcase_add_test(tc, test_remove);
	tcase_add_test(tc, test_remove_dir);
	tcase_add_test(tc, test_remove_type);
	tcase_add_test(tc, get_currdir_wbuffer);
	tcase_add_test(tc, get_currdir_tooshort);
	tcase_add_test(tc, get_currdir_malloc);

	if (has_unprivileged_symlinks)
		tcase_add_test(tc, test_remove_rec);

	return tc;
}
