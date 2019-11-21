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
#include <string.h>

#include "mmlib.h"
#include "mmlog.h"
#include "mmdlfcn.h"
#include "mmsysio.h"
#include "mmerrno.h"
#include "mmtime.h"
#include "api-testcases.h"

#define DEST_DIR	BUILDDIR"/test_directory_"
#define SRC_DIR	BUILDDIR"/handmaid/"LT_OBJDIR

#define BINARY_PATH	SRC_DIR"/handmaid"EXEEXT
#define CPY_BINARY_PATH	"/handmaid"EXEEXT
#if defined(_WIN32)
# define SHARED_LIB_PATH SRC_DIR"/libhandmaid-1"LT_MODULE_EXT
# define CPY_SHARED_LIB_PATH "/libhandmaid-1"LT_MODULE_EXT
#else
# define SHARED_LIB_PATH SRC_DIR"/libhandmaid"LT_MODULE_EXT".1.0.0"
# define CPY_SHARED_LIB_PATH "/libhandmaid"LT_MODULE_EXT".1"
#endif

#define DEST_DIR_SIZE sizeof(DEST_DIR) + 11
#define BINARY_NAME_SIZE sizeof(DEST_DIR) + 11 + sizeof(CPY_BINARY_PATH)
#define SHARED_LIB_NAME_SIZE sizeof(DEST_DIR) + 11 + sizeof(CPY_SHARED_LIB_PATH)

struct context {
	mm_pid_t pid;
	int r;
	int father_to_son;
	int son_to_father;
	char dir_name[DEST_DIR_SIZE];
	char binary_name[BINARY_NAME_SIZE];
	char shared_lib_name[SHARED_LIB_NAME_SIZE];
	char * env;
	int is_init;
};

static struct context context = { .is_init = 0, };


static
void cpy_file(const char * file_to_cpy, const char * cpy)
{
	struct mm_stat stat;
	int fd_to_cpy, fd_cpy;
	char * buf;

	fd_to_cpy = mm_open(file_to_cpy, O_RDONLY, 0666);
	mm_check(fd_to_cpy != -1);

	fd_cpy = mm_open(cpy, O_CREAT|O_TRUNC|O_WRONLY, 0777);
	mm_check(fd_cpy != -1);

	mm_check(mm_fstat(fd_to_cpy, &stat) == 0);

	mm_check(stat.size > 0);
	buf = malloc(stat.size);
	mm_check(buf != NULL);

	mm_check(mm_read(fd_to_cpy, buf, stat.size) > 0);
	mm_check(mm_write(fd_cpy, buf, stat.size) > 0);

	free(buf);
	mm_close(fd_to_cpy);
	mm_close(fd_cpy);
}


static
void create_proc(mm_pid_t* pid_ptr, struct context * c)
{
	int fd_son_to_father[2], fd_father_to_son[2];
	struct mm_remap_fd fdmap[2];
	char* argv[2];
	char started;

	// creation of anonymous pipes
	mm_check(mm_pipe(fd_son_to_father) == 0);
	mm_check(mm_pipe(fd_father_to_son) == 0);

	c->father_to_son = fd_father_to_son[1];
	c->son_to_father = fd_son_to_father[0];

	// Configure fdmap to keep all fd of the pipe in child
	fdmap[0].child_fd = 1;
	fdmap[0].parent_fd = fd_son_to_father[1];
	fdmap[1].child_fd = 0;
	fdmap[1].parent_fd = fd_father_to_son[0];

	argv[0] = c->binary_name;
	argv[1] = NULL;

	// Start process
	mm_check(mm_spawn(pid_ptr, argv[0], 2, fdmap, 0, argv, NULL) == 0);

	mm_close(fd_son_to_father[1]);
	mm_close(fd_father_to_son[0]);

	// wait for the son to start its execution
	mm_read(c->son_to_father, &started, sizeof(char));
}


static
int context_init(struct context * c)
{
	const char * ld_path_env;

	srand(clock());
	c->r = rand();
	sprintf(c->dir_name, "%s%d", DEST_DIR, c->r);

	mm_mkdir(c->dir_name, 0777, O_CREAT);

	sprintf(c->binary_name, "%s%d%s", DEST_DIR, c->r, CPY_BINARY_PATH);
	sprintf(c->shared_lib_name, "%s%d%s", DEST_DIR, c->r,
	        CPY_SHARED_LIB_PATH);

	// copy the executable and shared library
	cpy_file(BINARY_PATH, c->binary_name);
	cpy_file(SHARED_LIB_PATH, c->shared_lib_name);

	// save old environment
	ld_path_env = mm_getenv("LD_LIBRARY_PATH", NULL);
	if (ld_path_env)
		c->env = strdup(ld_path_env);

	// set new environment
	mm_setenv("LD_LIBRARY_PATH", c->dir_name, MM_ENV_PREPEND);

	create_proc(&c->pid, c);

	c->is_init = 1;

	return 0;
}


static
void context_deinit(struct context * c)
{
	char end = 'b';

	if (!c->is_init)
		return;

	// indicate the son that he can finish its execution
	mm_write(c->father_to_son, &end, sizeof(char));

	mm_remove(c->dir_name, MM_RECURSIVE|MM_DT_DIR|MM_DT_REG);

	mm_wait_process(c->pid, NULL);

	// close the pipes
	mm_close(c->father_to_son);
	mm_close(c->son_to_father);

	if (c->env)
		mm_setenv("LD_LIBRARY_PATH", c->env, MM_ENV_OVERWRITE);
	else
		mm_unsetenv("LD_LIBRARY_PATH");

	free(c->env);
	*c = (struct context) {
		.father_to_son = -1,
		.son_to_father = -1,
		.is_init = 0,
	};
}


static
void teardown_tests(void)
{
	int flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);

	context_deinit(&context);
	mm_error_set_flags(flags, MM_ERROR_IGNORE);
}


static
void setup_tests(void)
{
	mm_check(context_init(&context) == 0);
}


START_TEST(unlink_exec_linked_with_shared_lib)
{
	// unlink the file while the son is executing its code
	ck_assert(mm_unlink(context.binary_name) == 0);

	//check that the unlink really works
	ck_assert(mm_check_access(context.binary_name, F_OK) == ENOENT);
}
END_TEST


START_TEST(unlink_shared_lib_while_used_by_exec)
{
	// unlink the file while the son is executing its code
	ck_assert(mm_unlink(context.shared_lib_name) == 0);

	//check that the unlink really works
	ck_assert(mm_check_access(context.shared_lib_name, F_OK) == ENOENT);
}
END_TEST


START_TEST(unlink_opened_shared_lib)
{
	mmdynlib_t * shared_lib;

	shared_lib = mm_dlopen(context.shared_lib_name, MMLD_NOW);

	// unlink the file while the shared library is opened
	ck_assert(mm_unlink(context.shared_lib_name) == 0);

	//check that the unlink really works
	ck_assert(mm_check_access(context.shared_lib_name, F_OK) == ENOENT);

	mm_dlclose(shared_lib);
}
END_TEST


LOCAL_SYMBOL
TCase* create_advanced_file_tcase(void)
{
	TCase *tc = tcase_create("advanced_file_tcase");

	tcase_add_checked_fixture(tc, setup_tests, teardown_tests);

	tcase_add_test(tc, unlink_exec_linked_with_shared_lib);
	tcase_add_test(tc, unlink_shared_lib_while_used_by_exec);
	tcase_add_test(tc, unlink_opened_shared_lib);

	return tc;
}
