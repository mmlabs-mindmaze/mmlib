/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "api-testcases.h"
#include "mmargparse.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "mmsysio.h"

#include <check.h>
#include <stdlib.h>
#include <stdio.h>

#define LOREM_IPSUM "Lorem ipsum dolor sit amet, consectetur adipiscing"   \
"elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua." \
"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi " \
"ut aliquip ex ea commodo consequat..."

#define NARGV_MAX	10

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#define COMP_TREE_TESTDIR BUILDDIR"/completion_testdir"

struct argv_case {
	char* argv[NARGV_MAX];
	const char* expval;
};


/**
 * argv_len() - return the number of argument in a NULL terminated array
 * @argv:       NULL terminated array of argument strings
 *
 * Return: the number of element in @argv, excluding NULL termination
 */
static
int argv_len(char* argv[])
{
	int len = 0;

	while (argv[len] != NULL)
		len++;

	return len;
}

/**
 * fork_enabled() - return 1 if tests are executing through fork() calls
 *                  and 0 otherwise
 *
 * Return: 1 or 0 depending on the activation of fork() for tests
 */
static
int fork_enabled(void)
{
	const char *s;
#ifdef _WIN32
	return 0;
#endif
	s = mm_getenv("CK_FORK", NULL);
	if (s)
		return 1;
	return 0;
}

/**************************************************************************
 *                                                                        *
 *                           setup/teardown of tests                      *
 *                                                                        *
 **************************************************************************/
static int prev_stdout_fd;
static int rw_out_fd;
static FILE* read_out_fp;
static char* prev_cwd;

static
const char* completion_tree[] = {
	"adir/yet_another_file",
	"afile",
	"anotherdir/bar",
	"anotherdir/foo",
	"emptydir/",
	"file_1",
	"file_2",
};


/**
 * create_completion_test_tree() - create a completion test tree if folder
 * @testdir:    folder where to create the completion tree (will be wiped
 *              if it already exist).
 */
static
void create_completion_test_tree(const char* testdir)
{
	char path[128], dirpath[128];
	int i, len, fd;

	mm_remove(testdir, MM_RECURSIVE|MM_DT_ANY);

	for (i = 0; i < MM_NELEM(completion_tree); i++) {
		sprintf(path, "%s/%s", testdir, completion_tree[i]);
		len = strlen(path);

		// If this is folder name, create only a folder
		if (path[len-1] == '/')
			mm_mkdir(path, 0777, MM_RECURSIVE);

		// Create parent dir
		mm_dirname(dirpath, path);
		mm_mkdir(dirpath, 0777, MM_RECURSIVE);

		// Create file
		fd = mm_open(path, O_CREAT|O_WRONLY, 0666);
		mm_close(fd);
	}
}


/**
 * reset_output_fd() - reset file of redirected output
 *
 * This function cleanup the content of file receiving the redirected
 * standard output and reset its file pointer.
 */
static
void reset_output_fd(void)
{
	fseek(read_out_fp, 0, SEEK_SET);
	mm_ftruncate(rw_out_fd, 0);
}


/**
 * argparse_case_setup() - global setup of argparse tests
 *
 * This backups the current standard output object and create a file that
 * will receive the redirected standard output during the tests
 */
static
void argparse_case_setup(void)
{
	// Save standard output object
	prev_stdout_fd = mm_dup(STDOUT_FILENO);

	// Create file that will receive the standard output of test
	rw_out_fd = mm_open(BUILDDIR"/argparse.out",
	                      O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
	read_out_fp = fdopen(rw_out_fd, "r");

	// Create completion test dir and change current directory for it
	create_completion_test_tree(COMP_TREE_TESTDIR);
	prev_cwd = mm_getcwd(NULL, 0);
	mm_chdir(COMP_TREE_TESTDIR);
}


/**
 * argparse_case_teardown() - global cleanup of argparse tests
 *
 * Restore the standard output as it was before running the argparse test
 * cases and close the file receiving the redirected  standard output
 */
static
void argparse_case_teardown(void)
{
	// Close standard output object backup
	mm_close(prev_stdout_fd);

	// This will close rw_out_fd as well
	fclose(read_out_fp);

	// Remove completion testdir and restore current directory
	mm_remove(COMP_TREE_TESTDIR, MM_RECURSIVE|MM_DT_ANY);
	mm_chdir(prev_cwd);
	free(prev_cwd);
}


/**
 * each_test_setup() - setup run before each argparse test
 *
 * This test setup reset (empty) the content of the redirected standard
 * output. Also it restablish the redirection standard output to file. This
 * redirection is necessary to catch and inspect the standard output produced
 * during the test.
 */
static
void each_test_setup(void)
{
	// Since we are going to change the standard output object, we must
	// flush any pending data in stdout file stream (the thing behind
	// printf()), otherwise previous write might occurs on redirected
	// object. In particular, without flush, TAP report may appear in
	// the wrong place.
	fflush(stdout);

	reset_output_fd();

	// Connect standard output to a file we can read
	mm_dup2(rw_out_fd, STDOUT_FILENO);
}


/**
 * each_test_teardown() - cleanup run after each argparse test
 *
 * This test setup restore the standard output to its initial object. This
 * is necessary to communicate the test result to the upper layers.
 */
static
void each_test_teardown(void)
{
	// Same as setup counterpart, we change object behind STDOUT_FILENO
	// file descriptor, so we need to flush stdout file stream
	// beforehand.
	fflush(stdout);

	// Restore previous standard output
	mm_dup2(prev_stdout_fd, STDOUT_FILENO);
}


/**************************************************************************
 *                                                                        *
 *                           argument parsing tests                       *
 *                                                                        *
 **************************************************************************/
#define STRVAL_UNSET    u8"unset_value"
#define STRVAL_DEFAULT  u8"default_value"
#define STRVAL1         u8"skljfhls"
#define STRVAL2         u8"é(è-d--(è"
#define STRVAL3         u8"!:;mm"
#define STRVAL4         u8"µ%POPIP"

static
struct mm_arg_opt cmdline_optv[] = {
	{"d|distractor", MM_OPT_OPTSTR, "default_distractor", {NULL}, NULL},
	{"s|set", MM_OPT_OPTSTR, STRVAL_DEFAULT, {NULL}, NULL},
};

static const
struct argv_case parsing_order_cases[] = {
	{{"prg_name", "-s", STRVAL1, NULL}, STRVAL1},
	{{"prg_name", "-s", STRVAL1, "-s", STRVAL2, NULL}, STRVAL2},
	{{"prg_name", "-s", STRVAL1, "-s", NULL}, STRVAL_DEFAULT},
	{{"prg_name", "--set="STRVAL3, NULL}, STRVAL3},
	{{"prg_name", "--set", STRVAL3, NULL}, STRVAL_DEFAULT},
	{{"prg_name", "-s", STRVAL1, "--set="STRVAL3, NULL}, STRVAL3},
	{{"prg_name", "-s", STRVAL1, "an_argument", "--set="STRVAL3, NULL}, STRVAL1},
	{{"prg_name", "-s", STRVAL1, "--", "--set="STRVAL3, NULL}, STRVAL1},
	{{"prg_name", "--set="STRVAL3, "-s", STRVAL4, NULL}, STRVAL4},
	{{"prg_name", "-d", STRVAL1, "-s", STRVAL2, NULL}, STRVAL2},
	{{"prg_name", "-s", STRVAL1, "-d", STRVAL2, NULL}, STRVAL1},
	{{"prg_name", "-s", STRVAL1, "--set="STRVAL4, NULL}, STRVAL4},
	{{"prg_name", NULL}, STRVAL_UNSET},
};

static
int parse_option_cb(const struct mm_arg_opt* opt, union mm_arg_val value,
                    void* data, int state)
{
	(void)state;
	const char** strval = data;

	if (mm_arg_opt_get_key(opt) == 's')
		*strval = value.str;

	return 0;
}


START_TEST(parsing_order_cb)
{
	const char* str_value = STRVAL_UNSET;
	struct mm_arg_parser parser = {
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.cb = parse_option_cb,
		.cb_data = &str_value,
	};
	int arg_index, argc;
	const struct argv_case* case_data = &parsing_order_cases[_i];
	char** argv = (char**)case_data->argv;

	argc =  argv_len(argv);
	arg_index = mm_arg_parse(&parser, argc, argv);

	ck_assert_int_ge(arg_index, 0);
	ck_assert_str_eq(str_value, case_data->expval);
}
END_TEST

/**************************************************************************
 *                                                                        *
 *                        argument validation tests                       *
 *                                                                        *
 **************************************************************************/
static
struct {
	long long ll;
	unsigned long long ull;
	int i;
	unsigned int ui;
	const char* str;
} optdata = {0};

static
struct mm_arg_opt argval_valid_tests_optv[] = {
	{"set-ll", MM_OPT_NEEDLLONG, NULL, {.llptr = &optdata.ll}, NULL},
	{"set-ull", MM_OPT_NEEDULLONG, NULL, {.ullptr = &optdata.ull},
	 "Use this option to ull to @VAL_ULL. Recall value is @VAL_ULL."},
	{"i|set-i", MM_OPT_NEEDINT, NULL, {.iptr = &optdata.i}, NULL},
	{"set-ui", MM_OPT_NEEDUINT, NULL, {.uiptr = &optdata.ui}, NULL},
	{"set-str", MM_OPT_NEEDSTR, NULL, {.sptr = &optdata.str}, NULL},
};


static const
struct argv_case help_argv_cases[] = {
	{.argv = {"prg_name", "-h"}},
	{.argv = {"prg_name", "--help"}},
	{.argv = {"prg_name", "--help", "hello"}},
	{.argv = {"prg_name", "--set-ll=-1", "--help", "hello"}},
	{.argv = {"prg_name", "-h", "hello"}},
	{.argv = {"prg_name", "--set-ll=-1", "-h", "hello"}},
};

START_TEST(print_help)
{
	struct mm_arg_parser parser = {
		.flags = MM_ARG_PARSER_NOEXIT,
		.doc = LOREM_IPSUM,
		.optv = argval_valid_tests_optv,
		.num_opt = MM_NELEM(argval_valid_tests_optv),
	};
	int argc, rv;
	char** argv = (char**)help_argv_cases[_i].argv;

	argc = argv_len(argv);
	rv = mm_arg_parse(&parser, argc, argv);

	ck_assert_int_eq(rv, MM_ARGPARSE_STOP);
}
END_TEST

#define INT_TOOBIG      "2147483648"            // INT32_MAX+1
#define INT_TOOSMALL    "-2147483649"           // INT32_MIN-1
#define UINT_TOOBIG     "4294967296"            // UINT32_MAX+1
#define LLONG_TOOBIG    "9223372036854775808"   // INT64_MAX+1
#define LLONG_TOOSMALL  "-9223372036854775809"  // INT64_MIN-1
#define ULLONG_TOOBIG   "18446744073709551616"  // UINT64_MAX+1

static const
struct argv_case error_argv_cases[] = {
	{.argv = {"prg_name", "-k"}},
	{.argv = {"prg_name", "-i"}},
	{.argv = {"prg_name", "-i", "-o"}},
	{.argv = {"prg_name", "---set-ll=-1"}},
	{.argv = {"prg_name", "--unknown-opt"}},
	{.argv = {"prg_name", "--set-ll=-1",  "--unknown-opt"}},
	{.argv = {"prg_name", "-i", "42",  "--unknown-opt"}},
	{.argv = {"prg_name", "--set-i=not_a_number"}},
	{.argv = {"prg_name", "--set-i=21_noise"}},
	{.argv = {"prg_name", "--set-i="INT_TOOBIG}},
	{.argv = {"prg_name", "--set-i="INT_TOOSMALL}},
	{.argv = {"prg_name", "--set-ui="UINT_TOOBIG}},
	{.argv = {"prg_name", "--set-ui=-1"}},
	{.argv = {"prg_name", "--set-ll="LLONG_TOOBIG}},
	{.argv = {"prg_name", "--set-ll="LLONG_TOOSMALL}},
	{.argv = {"prg_name", "--set-ull="ULLONG_TOOBIG}},
	{.argv = {"prg_name", "--set-ull=-1"}},
};

START_TEST(parsing_error)
{
	struct mm_arg_parser parser = {
		.flags = MM_ARG_PARSER_NOEXIT,
		.optv = argval_valid_tests_optv,
		.num_opt = MM_NELEM(argval_valid_tests_optv),
	};
	int argc, rv;
	char** argv = (char**)error_argv_cases[_i].argv;

	argc = argv_len(argv);
	rv = mm_arg_parse(&parser, argc, argv);

	ck_assert_int_eq(rv, MM_ARGPARSE_ERROR);
}
END_TEST


START_TEST(optv_parsing_error)
{
	int argc;
	char** argv = (char**)error_argv_cases[_i].argv;

	argc = argv_len(argv);
	mm_arg_optv_parse(MM_NELEM(argval_valid_tests_optv),
	                 argval_valid_tests_optv, argc, argv);
}
END_TEST


static const
struct argv_case success_argv_cases[] = {
	{.argv = {"prg_name", "an_arg", "--unknown-opt"}, "an_arg"},
	{.argv = {"prg_name", "--", "--unknown-opt"}, "--unknown-opt"},
	{.argv = {"prg_name", "-", "--unknown-opt"}, "-"},
	{.argv = {"prg_name", "-i", "42", "--", "--unknown-opt"}, "--unknown-opt"},
	{.argv = {"prg_name", "-i", "42", "another --arg"}, "another --arg"},
};

START_TEST(parsing_success)
{
	struct mm_arg_parser parser = {
		.flags = MM_ARG_PARSER_NOEXIT,
		.optv = argval_valid_tests_optv,
		.num_opt = MM_NELEM(argval_valid_tests_optv),
	};
	int argc, rv;
	char** argv = (char**)success_argv_cases[_i].argv;

	argc = argv_len(argv);
	rv = mm_arg_parse(&parser, argc, argv);

	ck_assert_int_ge(rv, 0);
	ck_assert_str_eq(argv[rv], success_argv_cases[_i].expval);
}
END_TEST

START_TEST(optv_parsing_success)
{
	int argc, rv;
	char** argv = (char**)success_argv_cases[_i].argv;

	argc = argv_len(argv);
	rv = mm_arg_optv_parse(MM_NELEM(argval_valid_tests_optv),
	                      argval_valid_tests_optv, argc, argv);

	ck_assert_int_ge(rv, 0);
	ck_assert_str_eq(argv[rv], success_argv_cases[_i].expval);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                  option name and shortkey parsing tests                *
 *                                                                        *
 **************************************************************************/
struct {
	struct mm_arg_opt opt;
	char key;
	char* name;
} parse_optname_cases [] = {
	{{.name = "d"}, .key = 'd', .name = NULL},
	{{.name = "choice"}, .key = 0, .name = "choice"},
	{{.name = "d|choice"}, .key = 'd', .name = "choice"},
	{{.name = "a-choice"}, .key = 0, .name = "a-choice"},
	{{.name = "a|a-choice"}, .key = 'a', .name = "a-choice"},
};


START_TEST(get_key)
{
	int key;
	const struct mm_arg_opt* opt = &parse_optname_cases[_i].opt;

	key = mm_arg_opt_get_key(opt);

	ck_assert_int_eq(key, parse_optname_cases[_i].key);
}
END_TEST


START_TEST(get_name)
{
	const char *name, *ref_name;
	const struct mm_arg_opt* opt = &parse_optname_cases[_i].opt;

	name = mm_arg_opt_get_name(opt);
	ref_name = parse_optname_cases[_i].name;

	if (ref_name)
		ck_assert_str_eq(name, ref_name);
	else
		ck_assert(name == NULL);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                           completion tests                             *
 *                                                                        *
 **************************************************************************/

// Given an number of option, the number N of completion proposal can be up
// to 2*N + 2 (short option and long option) and -h and --help. Also We need
// to count an additional one for the NULL terminator.
#define NMAX_PROPS (2*MM_NELEM(argval_valid_tests_optv) + 2 + 1)
struct {
	char* arg;
	char* props[NMAX_PROPS];
} comp_cases[] = {
	{"-", {"-h", "--help", "--set-ll=", "--set-ull=", "-i", "--set-i=", "--set-ui=", "--set-str="}},
	{"--", {"--help", "--set-ll=", "--set-ull=", "--set-i=", "--set-ui=", "--set-str="}},
	{"--set", {"--set-ll=", "--set-ull=", "--set-i=", "--set-ui=", "--set-str="}},
	{"--set-u", {"--set-ull=", "--set-ui="}},
	{"--set-ul", {"--set-ull="}},
	{"--set-ull=", {""}},
	{"--set-ull=123", {"123"}},
};


/**
 * check_props_from_output_file() - verify completion proposal are present
 * @expected_props:     array of expected completion proposal (NULL term)
 *
 * This function verifies that the lines sent to standard output (which has
 * been redirected to a file) correspond to the expected completion proposal
 * listed in @expected_props. The completion proposal in the standard output
 * do not need to appear in the same order as in @expected_props.
 */
static
void check_props_from_output_file(char* expected_props[])
{
	char prop[128];
	int expected_prop_seen[NMAX_PROPS] = {0};
	int n_expected, n_seen, i, len;

	n_expected = argv_len(expected_props);
	n_seen = 0;

	// Flush standard output to ensure that content accessible through
	// read_out_fp is complete
	fflush(stdout);

	fseek(read_out_fp, 0, SEEK_SET);

	// loop over of the line of the file
	while (fgets(prop, sizeof(prop), read_out_fp)) {
		// Remove final end of line (if any)
		len = strlen(prop);
		if (prop[len-1] == '\n')
			prop[len-1] = '\0';

		// Search the line in the expected props
		for (i = 0; i < n_expected; i++) {
			if (!strcmp(prop, expected_props[i]))
				break;
		}

		// Fail if line could have not been found
		ck_assert_msg(i != n_expected, "\"%s\" not found", prop);

		n_seen++;

		// Check we have not already seen the expected prop
		ck_assert(expected_prop_seen[i]++ == 0);
	}

	// verify that we have not missed any expected prop
	ck_assert_int_eq(n_seen, n_expected);
}


START_TEST(complete_empty_arg)
{
	struct mm_arg_parser parser = {
		.flags = MM_ARG_PARSER_NOEXIT | MM_ARG_PARSER_COMPLETION,
		.optv = argval_valid_tests_optv,
		.num_opt = MM_NELEM(argval_valid_tests_optv),
	};
	char* only_argv[] = {"bin", ""};
	char* few_argv[] = {"bin", "--set-ll=42", "-i", "-23", ""};
	int rv;

	rv = mm_arg_parse(&parser, MM_NELEM(only_argv), only_argv);
	ck_assert_int_eq(rv, MM_NELEM(only_argv)-1);

	rv = mm_arg_parse(&parser, MM_NELEM(few_argv), few_argv);
	ck_assert_int_eq(rv, MM_NELEM(few_argv)-1);
}
END_TEST


START_TEST(complete_opt)
{
	struct mm_arg_parser parser = {
		.flags = MM_ARG_PARSER_NOEXIT | MM_ARG_PARSER_COMPLETION,
		.optv = argval_valid_tests_optv,
		.num_opt = MM_NELEM(argval_valid_tests_optv),
	};
	char** expected_props = comp_cases[_i].props;
	char* only_argv[] = {"bin", comp_cases[_i].arg};
	char* few_argv[] = {"bin", "--set-ll=42", comp_cases[_i].arg};
	int rv;

	rv = mm_arg_parse(&parser, MM_NELEM(only_argv), only_argv);
	ck_assert_int_eq(rv, MM_ARGPARSE_COMPLETE);
	check_props_from_output_file(expected_props);

	reset_output_fd();

	rv = mm_arg_parse(&parser, MM_NELEM(few_argv), few_argv);
	ck_assert_int_eq(rv, MM_ARGPARSE_COMPLETE);
	check_props_from_output_file(expected_props);
}
END_TEST


struct {
	char* arg;
	char* props[MM_NELEM(completion_tree)];
} comp_path_cases[] = {
	{"", {"adir/", "afile", "anotherdir/", "emptydir/", "file_1", "file_2"}},
	{"a", {"adir/", "afile", "anotherdir/"}},
	{"f", {"file_1", "file_2"}},
	{"an", {"anotherdir/"}},
	{"anotherdir", {"anotherdir/"}},
	{"anotherdir/", {"anotherdir/bar", "anotherdir/foo"}},
	{"emptydir/", {0}},
};


/**
 * filter_props_type() - copy elements of array that match typemask
 * @dst_props:       destination array
 * @src_props:       source array
 * @typemask:        mask of type flags (MM_DT_*)
 */
static
void filter_props_type(char** dst_props, char** src_props, int typemask)
{
	struct mm_stat st;
	int i, j;

	j = 0;
	for (i = 0; src_props[i]; i++) {
		// Add to filtered proposal only it type match
		if (mm_stat(src_props[i], &st, MM_NOFOLLOW))
			continue;

		if ((S_ISREG(st.mode) && (typemask & MM_DT_REG))
		   || (S_ISDIR(st.mode) && (typemask & MM_DT_DIR))
		   || (S_ISLNK(st.mode) && (typemask & MM_DT_LNK)))
			dst_props[j++] = src_props[i];
	}

	dst_props[j] = NULL;
}


/**
 * filt_name_path_cb() - test whether name start string passed in data
 * @name:       basename of completion proposal
 * @dir:        dirname of completion proposal
 * @type:       type of completion proposal
 * @data:       pointer passed to mm_arg_complete_path()... must hold the
 *              string beginning to check
 *
 * function used to test the callback mechanism of mm_arg_complete_path().
 * It checks that completion candidate has its basename (@name) that start
 * with a string specified by @data.
 */
static
int filt_name_path_cb(const char* name, const char* dir, int type, void* data)
{
	const char* start = data;
	(void) type;
	(void) dir;

	return (strncmp(start, name, strlen(start)) == 0);
}


/**
 * filter_props_type() - filter array whose basename has the expected start
 * @dst_props:       destination array
 * @src_props:       source array
 * @start:           string matching the beginning of basename of elements
 */
static
void filter_props_name(char** dst_props, char** src_props, const char* start)
{
	char base[64];
	int i, j, len;

	len = strlen(start);

	j = 0;
	for (i = 0; src_props[i]; i++) {
		mm_basename(base, src_props[i]);
		if (strncmp(start, base, len) == 0)
			dst_props[j++] = src_props[i];
	}

	dst_props[j] = NULL;
}


START_TEST(complete_path)
{
	char* expected_props[MM_NELEM(comp_path_cases[0].props)];
	char** expfull_props = comp_path_cases[_i].props;
	char* arg = comp_path_cases[_i].arg;
	int typemasks[] = {MM_DT_ANY, MM_DT_REG, MM_DT_DIR};
	int i, mask, rv;

	for (i = 0; i < MM_NELEM(typemasks); i++) {
		// Consider only proposal matching typemask
		mask = typemasks[i];
		filter_props_type(expected_props, expfull_props, mask);

		// Complete path from arg and check its expected proposals
		rv = mm_arg_complete_path(arg, mask, NULL, NULL);
		ck_assert(rv == 0);
		check_props_from_output_file(expected_props);

		reset_output_fd();

		// limit further proposal whose basename start by "f"
		filter_props_name(expected_props, expected_props, "f");
		rv = mm_arg_complete_path(arg, mask, filt_name_path_cb, "f");
		ck_assert(rv == 0);
		check_props_from_output_file(expected_props);

		reset_output_fd();
	}
}
END_TEST


struct {
	char* argv[4];
	int opttype;
	char* props[MM_NELEM(completion_tree)];
} comp_argval_path_cases[] = {
	{
		{"prog", "--set-path="}, MM_OPT_NEEDFILE,
		{"adir/", "afile", "anotherdir/", "emptydir/", "file_1", "file_2"}
	}, {
		{"prog", "-p", ""}, MM_OPT_NEEDFILE,
		{"adir/", "afile", "anotherdir/", "emptydir/", "file_1", "file_2"}
	}, {
		{"prog", "--set-path="}, MM_OPT_NEEDDIR,
		{"adir/", "anotherdir/", "emptydir/"}
	}, {
		{"prog", "-p", ""}, MM_OPT_NEEDDIR,
		{"adir/", "anotherdir/", "emptydir/"}
	}, {
		{"prog", "--set-path=a"}, MM_OPT_NEEDFILE,
		{"adir/", "afile", "anotherdir/"}
	}, {
		{"prog", "-p", "a"}, MM_OPT_NEEDFILE,
		{"adir/", "afile", "anotherdir/"}
	}, {
		{"prog", "--set-path=a"}, MM_OPT_NEEDDIR,
		{"adir/", "anotherdir/"}
	}, {
		{"prog", "--set-path=foobar"}, MM_OPT_NEEDFILE, {0}
	},
};


START_TEST(complete_argval_path)
{
	char** expected_props = comp_argval_path_cases[_i].props;
	char** argv = comp_argval_path_cases[_i].argv;
	const char* str = NULL;
	struct mm_arg_opt optv[] = {
		{"p|set-path", comp_argval_path_cases[_i].opttype,
		 NULL, {.sptr = &str}, NULL},
	};
	struct mm_arg_parser parser = {
		.flags = MM_ARG_PARSER_NOEXIT | MM_ARG_PARSER_COMPLETION,
		.optv = optv, .num_opt = MM_NELEM(optv),
	};
	int rv;

	rv = mm_arg_parse(&parser, argv_len(argv), argv);
	ck_assert(rv == MM_ARGPARSE_COMPLETE);
	check_props_from_output_file(expected_props);

	// Ensure that str has NOT been set (completion must not set val)
	ck_assert(str == NULL);
}
END_TEST


#define INIT_IVAL 9999
static
struct mm_arg_opt completion_in_cb_optv[] = {
	{"d|distractor", MM_OPT_NEEDSTR, "default_distractor", {NULL}, NULL},
	{"i|set-i", MM_OPT_OPTINT, "-1", {NULL}, NULL},
};

static struct {
	char* argv[10];
	int exp_val;
	char* props[2*MM_NELEM(completion_in_cb_optv)+3];
} comp_in_cb_cases[] = {
	{{"prg_name", "--set-i="}, INIT_IVAL, {"512", "1024"}},
	{{"prg_name", "--set-i=42"}, INIT_IVAL, {"512", "1024"}},
	{{"prg_name", "--set-i=42", "--set-i=24"}, 42, {"512", "1024"}},
	{{"prg_name", "--set-i=42", "--"}, 42, {"--help", "--set-i", "--set-i=", "--distractor="}},
	{{"prg_name", "-i"}, INIT_IVAL, {"-i"}},
	{{"prg_name", "-i", "42"}, INIT_IVAL, {"512", "1024"}},
	{{"prg_name", "-i", "42", "-i", "24"}, 42, {"512", "1024"}},
	{{"prg_name", "-i", "42", "-i"}, 42, {"-i"}},
	{{"prg_name", "-i", "42"}, INIT_IVAL, {"512", "1024"}},
	{{"prg_name", "--set-i=42", "--distractor=24"}, 42, {"24"}},
	{{"prg_name", "-i", "42", "-d", "24"}, 42, {"24"}},
};


static
int comp_option_parse_cb(const struct mm_arg_opt* opt, union mm_arg_val value,
                         void* data, int state)
{
	int* ival = data;

	if (mm_arg_opt_get_key(opt) == 'i') {
		if (state & MM_ARG_OPT_COMPLETION) {
			printf("512\n1024\n");
			return MM_ARGPARSE_COMPLETE;
		}

		*ival = value.i;
	}

	return 0;
}


START_TEST(completion_in_cb)
{
	int i_value = INIT_IVAL;
	struct mm_arg_parser parser = {
		.flags = MM_ARG_PARSER_NOEXIT | MM_ARG_PARSER_COMPLETION,
		.optv = completion_in_cb_optv,
		.num_opt = MM_NELEM(completion_in_cb_optv),
		.cb = comp_option_parse_cb,
		.cb_data = &i_value,
	};
	int arg_index, argc;
	char** argv = comp_in_cb_cases[_i].argv;
	char** expected_props = comp_in_cb_cases[_i].props;

	argc = argv_len(argv);
	arg_index = mm_arg_parse(&parser, argc, argv);

	ck_assert_int_eq(arg_index, MM_ARGPARSE_COMPLETE);
	ck_assert_int_eq(i_value, comp_in_cb_cases[_i].exp_val);
	check_props_from_output_file(expected_props);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_argparse_tcase(void)
{
	TCase *tc = tcase_create("argparse");

	tcase_add_unchecked_fixture(tc, argparse_case_setup, argparse_case_teardown);
	tcase_add_checked_fixture(tc, each_test_setup, each_test_teardown);

	tcase_add_loop_test(tc, get_key, 0, MM_NELEM(parse_optname_cases));
	tcase_add_loop_test(tc, get_name, 0, MM_NELEM(parse_optname_cases));
	tcase_add_loop_test(tc, parsing_order_cb, 0, MM_NELEM(parsing_order_cases));
	tcase_add_loop_test(tc, print_help, 0, MM_NELEM(help_argv_cases));
	tcase_add_loop_test(tc, parsing_error, 0, MM_NELEM(error_argv_cases));
	tcase_add_loop_test(tc, parsing_success, 0, MM_NELEM(success_argv_cases));
	if (fork_enabled()) // needed as optv_parsing_error calls exit()
		tcase_add_loop_exit_test(tc, optv_parsing_error, EXIT_FAILURE, 0,
					 MM_NELEM(error_argv_cases));
	tcase_add_loop_test(tc, optv_parsing_success, 0,
                            MM_NELEM(success_argv_cases));
	tcase_add_test(tc, complete_empty_arg);
	tcase_add_loop_test(tc, complete_opt, 0, MM_NELEM(comp_cases));
	tcase_add_loop_test(tc, complete_path, 0, MM_NELEM(comp_path_cases));
	tcase_add_loop_test(tc, complete_argval_path,
	                    0, MM_NELEM(comp_argval_path_cases));
	tcase_add_loop_test(tc, completion_in_cb, 0, MM_NELEM(comp_in_cb_cases));

	return tc;
}
