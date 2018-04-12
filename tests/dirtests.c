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
#include <strings.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "mmerrno.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "mmsysio.h"

#define TMP_DIR_ROOT TOP_BUILDDIR "/mmlib-test-dir"
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


/* windows display files in alphabetical order
 * linux display files in time order */
#if defined (_WIN32)
#define TREE_REF\
	"├─ 1 (dir)\n\t├─ 2 (dir)\n\t\t├─ 3 (dir)\n\t\t\t├─ file1.dat (reg)\n├─ 4 (dir)\n\t├─ 5 (dir)\n\t\t├─ 6 (dir)\n\t\t\t├─ file1.lnk (link)\n"
#else /* _WIN32 */
#define TREE_REF\
	"├─ 4 (dir)\n\t├─ 5 (dir)\n\t\t├─ 6 (dir)\n\t\t\t├─ file1.lnk (link)\n├─ 1 (dir)\n\t├─ 2 (dir)\n\t\t├─ 3 (dir)\n\t\t\t├─ file1.dat (reg)\n"
#endif /* _WIN32 */
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
	ck_assert(strcmp(tree_ref, TREE_REF) == 0);

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

	tcase_add_test(tc, test_dir_create_rec);
	tcase_add_test(tc, test_dir_open_close);
	tcase_add_test(tc, test_dir_read);
	tcase_add_test(tc, test_remove);
	tcase_add_test(tc, test_remove_type);

	if (has_unprivileged_symlinks)
		tcase_add_test(tc, test_remove_rec);

	return tc;
}
