/*
 * @mindmaze_header@
 */
#ifndef MMARGPARSE_H
#define MMARGPARSE_H

#include <stddef.h>
#include "mmpredefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MMOPT_NOVAL    0x00
#define MMOPT_OPTVAL   0x01
#define MMOPT_NEEDVAL  0x02
#define MMOPT_REQMASK  0x03
#define MMOPT_STR      0x00
#define MMOPT_INT      0x10
#define MMOPT_LLONG    0x20
#define MMOPT_UINT     0x90
#define MMOPT_ULLONG   0xA0
#define MMOPT_TYPEMASK 0xF0
#define MMOPT_FILEPATH 0x100
#define MMOPT_DIRPATH  0x200

#define MMOPT_OPTSTR     (MMOPT_OPTVAL | MMOPT_STR)
#define MMOPT_NEEDSTR    (MMOPT_NEEDVAL | MMOPT_STR)
#define MMOPT_OPTINT     (MMOPT_OPTVAL | MMOPT_INT)
#define MMOPT_NEEDINT    (MMOPT_NEEDVAL | MMOPT_INT)
#define MMOPT_OPTLLONG   (MMOPT_OPTVAL | MMOPT_LLONG)
#define MMOPT_NEEDLLONG  (MMOPT_NEEDVAL | MMOPT_LLONG)
#define MMOPT_OPTUINT    (MMOPT_OPTVAL | MMOPT_UINT)
#define MMOPT_NEEDUINT   (MMOPT_NEEDVAL | MMOPT_UINT)
#define MMOPT_OPTULLONG  (MMOPT_OPTVAL | MMOPT_ULLONG)
#define MMOPT_NEEDULLONG (MMOPT_NEEDVAL | MMOPT_ULLONG)
#define MMOPT_OPTFILE    (MMOPT_OPTSTR | MMOPT_FILEPATH)
#define MMOPT_NEEDFILE   (MMOPT_NEEDSTR | MMOPT_FILEPATH)
#define MMOPT_OPTDIR     (MMOPT_OPTSTR | MMOPT_DIRPATH)
#define MMOPT_NEEDDIR    (MMOPT_NEEDSTR | MMOPT_DIRPATH)


/**
 * union mmarg_val - generic holder of argument value
 * @str:        pointer to string value
 * @i:          signed integer value
 * @ll:         signed long long value
 * @ui:         unsigned integer value
 * @ull:        unsigned long long value
 *
 * &union mmarg_val is the data holder to pass the value of an argument when a
 * argument callback function is called. The field to use to manipulate the
 * value depends on the type indicated &mmarg_opt.flags of the option supplied
 * in the callback. This type can be retrieved by the helper
 * mmarg_opt_get_type().
 */
union mmarg_val {
	const char* str;
	int i;
	long long ll;
	unsigned int ui;
	unsigned long long ull;
};


/**
 * struct mmarg_opt - option parser configuration
 * @name:       option names, see description
 * @flags:      conversion type and requirement flags, see options flags
 * @defval:     default value is option is supplied without value
 * @val:        union for the various types
 * @val.sptr:   pointer to pointer to string, used if type is MMOPT_STR
 * @val.iptr:   pointer to integer, used if type is MMOPT_INT
 * @val.llptr:  pointer to long long, used if type is MMOPT_LLONG
 * @val.uiptr:  pointer to unsigned integer, used if type is MMOPT_UINT
 * @val.ullptr: pointer to unsigned long long, used if type is MMOPT_ULLONG
 * @desc:       description of the option printed when usage is displayed
 *
 * This structure defines what an option supports and how its value must be
 * interpreted. @name defines both the short option key and long name. The
 * short key ("c") must be a single character of lower or upper case letter
 * in ascii character set. The long option name ("long-name") must contains
 * only ascii lower case letter, digit or hyphens. @name must adhere to one
 * the 3 following formats
 *
 * - "c|long-name": option can be referred by "-c" or "--long-name"
 * - "c": option can be referred only by short option "-c"
 * - "long-name": option can be referred only by long option "--long-name"
 *
 * The parameter @flags determine whether a value for an option:
 *
 * - %MMOPT_NOVAL: value is forbidden
 * - %MMOPT_OPTVAL: value is optional and @defval will be used if not set.
 * - %MMOPT_NEEDVAL: value is mandatory
 *
 * it also determines what is the supported type of a value (if not
 * forbidden):
 *
 * - %MMOPT_STR: value is string (then @val.sptr used)
 * - %MMOPT_INT: value is int (then @val.iptr used)
 * - %MMOPT_UINT: value is unsigned int (then @val.uiptr used)
 * - %MMOPT_LLONG: value is long long (then @val.llptr used)
 * - %MMOPT_ULLONG: value is unsigned long long (then @val.ullptr used)
 *
 * the ptr fields specified in parenthesis indicates that if the
 * corresponding field is not NULL, it will receive the value specified
 *
 * @desc provides the documentation of the option. It will be displayed if
 * usage is requested. Please note that if an option accepts a value
 * (optional or mandatory), the value name displayed in option synopsis
 * will be VALUE, unless, @desc contains one of more occurrence of string
 * following "@NAMEOFVALUE" pattern where NAMEOFVALUE is a string made of
 * only upper case letter and hyphens or underscore and of length less than
 * 16 characters. If such pattern can be found NAMEOFVALUE will be used for
 * synopsis and all occurrence of "@NAMEOFVALUE" will be replaced by
 * "NAMEOFVALUE" in description.
 */
struct mmarg_opt {
	const char* name;
	int flags;
	const char* defval;
	union {
		const char** sptr;
		int* iptr;
		long long* llptr;
		unsigned int* uiptr;
		unsigned long long* ullptr;
	} val;
	const char* desc;
};

#define MMARGPARSE_ERROR    -1
#define MMARGPARSE_STOP     -2
#define MMARGPARSE_COMPLETE -3

#define MMARG_OPT_COMPLETION (1 << 0)

/**
 * typedef mmarg_callback() - prototype of argument parser callback
 * @opt:        pointer to matching option
 * @value:      value set for option. The field of the union to use is
 *              determined with mmarg_opt_get_type(@opt).
 * @data:       user pointer provided for hold state while running parser
 * @state:      flags indicating the state of option parsing.
 *
 * The flags in @state indicates special calling context. It can be a
 * combination of the following flags :
 *
 * %MMARG_OPT_COMPLETION: the option value is being completed. The value
 * passed is then always a string, no matter is @opt->value. Also its
 * content may be incomplete. If the callback want to provide completion
 * candidates, it has to write them on standard output, each candidate on
 * its own line. If it returns MMARGPARSE_COMPLETE, no further completion
 * proposal will be added. This flag cannot appear if the flag
 * %MMARG_PARSER_COMPLETION has not been set in argument parser.
 *
 * Return: 0 in case of success, MMARGPARSE_ERROR (-1) if an error has been
 * detected, MMARGPARSE_STOP (-2) if early exit is requested like with help
 * display or MMARGPARSE_COMPLETE (-3) if value has been completed.
 */
typedef int (* mmarg_callback)(const struct mmarg_opt* opt,
                               union mmarg_val value, void* data, int state);

/**
 * typedef mmarg_complete_path_cb() - prototype of path completion callback
 * @name:       basename of path candidate
 * @dir:        directory of path candidate.
 * @type:       file type of path candidate (one of the MM_DT_* definition)
 * @data:       user pointer provided when calling path completion
 *
 * Return: 1 if path candidate must be kept, 0 if it must be discarded.
 */
typedef int (* mmarg_complete_path_cb)(const char* name, const char* dir,
                                       int type, void* data);

#define MMARG_PARSER_NOEXIT (1 << 0)
#define MMARG_PARSER_COMPLETION (1 << 1)

/**
 * struct mmarg_parser - argument parser configuration
 * @flags:      flags to change behavior of parsing (MMARG_PARSER_*)
 * @num_opt:    number of element in @optv.
 * @optv:       array of option supported. Please note that @optv does not
 *              need to provide an option "h|help" since argument parser add
 *              such option automatically which will trigger usage printing.
 *              You may however provide such option in @optv array to
 *              override the behaviour when this option is encountered.
 * @args_doc:   lines of synopsis of program usage (excluding program name).
 *              This can support multiple line (like for different case of
 *              invocation). Can be NULL.
 * @doc:        document of the program. Can be NULL
 * @execname:   name of executable. You are invited to set it to argv[0]. If
 *              NULL, "PROGRAM" will be used instead for synopsis
 * @cb_data:    user provided data callback
 * @cb:         callback called whenever an option is recognised and parsed.
 *              This callback is optional and can be set to NULL if
 *              unneeded.
 */
struct mmarg_parser {
	int flags;
	int num_opt;
	const struct mmarg_opt* optv;
	const char* doc;
	const char* args_doc;
	const char* execname;
	void* cb_data;
	mmarg_callback cb;
};


MMLIB_API int mmarg_parse(const struct mmarg_parser* parser,
                          int argc, char* argv[]);
MMLIB_API int mmarg_optv_parse(int optn, const struct mmarg_opt* optv,
                               int argc, char* argv[]);
MMLIB_API int mmarg_parse_complete(const struct mmarg_parser* parser,
                                   const char* arg);
MMLIB_API int mmarg_complete_path(const char* arg, int type_mask,
                                  mmarg_complete_path_cb cb, void* cb_data);
MMLIB_API int mmarg_is_completing(void);


/**
 * mmarg_opt_get_key() - get short key of an option
 * @opt:        option whose key must be returned
 *
 * Return: short key code used to recognised @opt if used, 0 if @opt cannot
 * be matched by short key.
 */
static inline
int mmarg_opt_get_key(const struct mmarg_opt* opt)
{
	char sep = opt->name[1];
	return (sep == '|' || sep == '\0') ? opt->name[0] : 0;
}


/**
 * mmarg_opt_get_name() - get long option name
 * @opt:        option whose long name must be returned
 *
 * Return: the long name of opt if supported, NULL otherwise.
 */
static inline
const char* mmarg_opt_get_name(const struct mmarg_opt* opt)
{
	int offset;
	char sep = opt->name[1];

	// Check the option is not identified solely with short key
	if (sep == '\0')
		return NULL;

	// The long name start depends whether the option can be identified
	// by short key or not
	offset = (sep == '|') ? 2 : 0;

	return opt->name + offset;
}


/**
 * mmarg_opt_get_type() - get type of value supported by an option
 * @opt:        option whose value type must be returned
 *
 * Return: MMOPT_STR, MMOPT_INT, MMOPT_UINT, MMOPT_LLONG or MMOPT_ULLONG
 */
static inline
int mmarg_opt_get_type(const struct mmarg_opt* opt)
{
	return opt->flags & MMOPT_TYPEMASK;
}


#ifdef __cplusplus
}
#endif

#endif /* ifndef MMARGPARSE_H */
