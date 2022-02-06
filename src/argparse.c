/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "file-internal.h"
#include "mmargparse.h"
#include "mmerrno.h"
#include "mmlib.h"
#include "mmsysio.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#define OPT_INDENT_LEN 30
#define LINE_MAXLENGTH 80

#define IGNORE_KEY 0

#define VALUE_NAME_MAXLEN 16
#define LONGOPT_NAME_MAXLEN 32
#define OPT_SYNOPSIS_MAXLEN (VALUE_NAME_MAXLEN + LONGOPT_NAME_MAXLEN+16)


static const struct mm_arg_opt help_opt = {
	.name = "h|help", .flags = MM_OPT_NOVAL,
	.desc = "print this message and exit",
};


struct value_type_name {
	int type;
	const char* name;
};

static
const struct value_type_name typenames[] = {
	{.type = MM_OPT_STR,    .name = "string"},
	{.type = MM_OPT_INT,    .name = "int"},
	{.type = MM_OPT_LLONG,  .name = "long long"},
	{.type = MM_OPT_UINT,   .name = "unsigned int"},
	{.type = MM_OPT_ULLONG, .name = "unsigned long long"},
};


static
const struct mm_arg_opt* find_opt(const struct mm_arg_parser* parser,
                                  int key, const char* name, int namelen);


/**
 * get_value_type_name() - get string describing type name
 * @type:	MM_OPT_* flags specifying a type
 *
 * Return: The type string, or "unknown" if not found.
 */
static
const char* get_value_type_name(int type)
{
	int i;

	for (i = 0; i < MM_NELEM(typenames); i++) {
		if (typenames[i].type == type)
			return typenames[i].name;
	}

	return "unknown";
}



/**
 * get_first_token_length() - get length of the first substring
 * @str:        null-terminated string to split
 * @breakch:    character at which @str must be split
 *
 * This function search for the first occurrence of @breakch and report the
 * length string if it were cut at this position. In any case, @str will
 * not be modified.
 *
 * Return: the length of the first substring found if breaking @str at
 * @breakch. If @breakch cannot be found, the full length of string will be
 * returned. In both case, the reported length excludes @breakch or the
 * null-termination.
 */
static
int get_first_token_length(const char* str, char breakch)
{
	char ch;
	int len = 0;

	ch = str[0];
	while (ch != '\0' && ch != breakch)
		ch = str[++len];

	return len;
}


/**
 * is_valid_short_opt_key() - check option key name belong to valid set
 * @ch:         character of short option key
 *
 * a valid key must be a letter in ascii set (upper or lower case)
 *
 * Return: true if key is valid, false otherwise
 */
static
bool is_valid_short_opt_key(int ch)
{
	return ((ch >= 'a' && ch <= 'z')
	        || (ch >= 'A' && ch <= 'Z'));
}


/**
 * is_valid_long_opt_name() - check long option name is valid
 * @name:       null terminated string of option name (without --)
 * @stop_at_equal: if true, stop inspecting @name once '=' is found
 *
 * a valid name contains only lower case letter in ascii set, numeric
 * characters and hyphens.
 *
 * Return: true if @name is valid, false otherwise
 */
static
bool is_valid_long_opt_name(const char* name, bool stop_at_equal)
{
	int i;
	char ch;

	// long name must start with letter
	ch = name[0];
	if (ch < 'a' || ch > 'z')
		return false;

	for (i = 1; name[i]; i++) {
		ch = name[i];

		if ((ch >= 'a' && ch <= 'z')
		    || (ch >= '0' && ch <= '9')
		    || (ch == '-'))
			continue;

		if (ch == '=' && stop_at_equal)
			break;

		return false;
	}

	// Final check: name must not be too short
	return (i >= 2) ? true : false;
}


/**
 * is_arg_an_option() - check string has a format of option
 * @arg:        null terminated string to test
 *
 * Return: true is @arg has the format of an option, false otherwise
 */
static
bool is_arg_an_option(const char* arg)
{
	if (!arg || arg[0] != '-')
		return false;

	// Check arg has short name option format
	if (arg[1] != '-' && arg[1] != '\0')
		return is_valid_short_opt_key(arg[1]);

	// Check arg has long name option format
	if (arg[2] != '-' && arg[2] != '\0')
		return is_valid_long_opt_name(arg+2, true);

	return false;
}


/**
 * is_char_valid_for_value_name() - check a char is valid for value name
 * @ch:         character to check
 *
 * Return: true if @ch is valid, false otherwise
 */
static
bool is_char_valid_for_value_name(char ch)
{
	if ((ch >= 'A' && ch <= 'Z')
	    || (ch == '-')
	    || (ch == '_'))
		return true;

	return false;
}


/*************************************************************************
 *                                                                       *
 *                       Help printing functions                         *
 *                                                                       *
 *************************************************************************/

/**
 * validate_value_name() - check whether string is suitable as value name
 * @str:        string to try
 *
 * Inspect the next character of @str and test whether they form a suitable
 * name for a value (in option description). A suitable name :
 * - is composed of capitals and '-' and '_' character,
 * - contains at least one valid character
 * - is not longer than VALUE_NAME_MAXLEN
 *
 * Since the value name is referenced with '@' prefixed in description,
 * this function is meant to be called when :
 *
 * 1. option description is scanned and no value name has been found yet.
 * 2. a '@' has been found in description
 *
 * Return: the length of the name if a suitable name has been found, 0
 * otherwise
 */
static
int validate_value_name(const char* str)
{
	int i;

	for (i = 0; (i < VALUE_NAME_MAXLEN) && (str[i] != '\0'); i++) {
		if (is_char_valid_for_value_name(str[i]))
			continue;

		// Value name must be at least one character
		if (i > 1)
			return i;
	}

	// name is too long, so invalid
	return 0;
}


/**
 * match_value() - test whether the string match a value name
 * @str:        null-terminated string to test
 * @valname:    value name to test
 * @namelen:    length of @valname
 *
 * Test if @str match @valname up to @namelen character AND if the next
 * character in @str if NOT character valid for a value name.
 *
 * Return: true in case of match, false otherwise
 */
static
bool match_value(const char* str, const char* valname, int namelen)
{
	int i;

	for (i = 0; i < namelen; i++) {
		if (valname[i] != str[i])
			return false;
	}

	// The match must be greedy, so if name could continue, we don't
	// have a match
	if (is_char_valid_for_value_name(str[namelen]))
		return false;

	return true;
}


/**
 * copy_opt_desc() - copy option description while guessing value name
 * @dst:       destination buffer where modified description is copied
 * @src:       description buffer as it is in &mm_arg_opt.desc
 * @buffer:    buffer that will hold the value name if found
 *
 * Copy option description from @src to @src while trying to guess the name
 * of option value as referred in the description. The value name will be
 * the first occurrence of a token of the form '@VALUENAME'. If one is
 * found, @buffer will be filled with the value name (null-terminated, but
 * without '@') and all occurrence of '@VALUENAME' in @src will be replace
 * in @dst by 'VALUENAME'.
 */
static
void copy_opt_desc(char* dst, const char* src, char* buffer)
{
	int namelen = 0;
	char* valname = NULL;
	const char* start = src;

	while (*src) {
		if ((*src == '@')
		    && ((start == src) || !isalnum(src[-1]))) {
			if (!valname) {
				namelen = validate_value_name(src+1);
				if (namelen) {
					// Store value name
					memcpy(buffer, src+1, namelen);
					buffer[namelen] = '\0';
					valname = buffer;

					// Copy value name to destination
					memcpy(dst, valname, namelen);
					dst += namelen;
					src += namelen+1;
					continue;
				}
			} else {
				if (match_value(src+1, valname, namelen)) {
					// Copy value name to destination
					memcpy(dst, valname, namelen);
					dst += namelen;
					src += namelen+1;
					continue;
				}
			}
		}

		*dst++ = *src++;
	}

	*dst = '\0';
}


/**
 * print_text_wrapped() - print wrapped text on stream with alignment
 * @line_maxlen:        maximum length of a line when printing
 * @text:               text to print
 * @align_len:          number of column to skip before printing @text
 * @header:             header to print only the first line (in alignment
 *                      zone). This may be NULL and would be treated as ""
 *                      in such a case.
 * @stream:             output stream on @text must be written
 *
 * This function print @text on standard error ensuring that each displayed
 * line will not be longer than @line_maxlen, wrapping text if necessary at
 * whitespace boundaries. The wrapping will respect the linefeed set in
 * @text. In addition the content of @text will be displayed after applying
 * an indent of @align_len character. Moreover, if @header is not null, it
 * will be printed on the first line displayed, in the indentation zone.
 *
 * As an example, here is how the LOREM_IPSUM string would be displayed
 * with print_text_wrapped(60, LOREM_IPSUM, 10, "head_str") :
 *
 * head_str   Lorem ipsum dolor sit amet, consectetur adipiscing
 *            elit, sed do eiusmod tempor incididunt ut labore
 *            et dolore magna aliqua. Ut enim ad minim veniam,
 *            quis nostrud exercitation ullamco laboris nisi ut
 *            aliquip ex ea commodo consequat...
 */
static
void print_text_wrapped(int line_maxlen, const char* text,
                        int align_len, const char* header, FILE* stream)
{
	int len, textline_maxlen;

	if (!header)
		header = "";

	textline_maxlen = line_maxlen - align_len;
	do {
		// Get the length of the next part to print
		len = get_first_token_length(text, '\n');

		// Check whether the text is too long and must be wrapped
		if (len >= textline_maxlen) {
			// Find a good place to split the string
			len = textline_maxlen;
			while (text[len] != ' ') {
				if (--len == 0) {
					len = textline_maxlen;
					break;
				}
			}
		}

		fprintf(stream, "%-*s%.*s\n", align_len, header, len, text);

		// Skip first linefeed since it has been displayed
		// already by the previous fprintf()
		if (text[len] == '\n')
			len++;

		// skip spaces
		while (text[len] == ' ')
			len++;

		text += len;
		header = "";
	} while (*text != '\0');
}


/**
 * print_synopsis() - print program synopsis on stream
 * @parser:     argument parser
 * @stream:     output stream on which the synopsis must be written
 *
 * Print on the standard error the synopsis of the program. If
 * @parser->execname is not set, "PROGRAM" will be used instead. For each
 * line in @parser->args_doc, a synopsis line is expanded. If
 * @parser->args_doc is NULL, a generic synopsis in printed.
 */
static
void print_synopsis(const struct mm_arg_parser* parser, FILE* stream)
{
	int len = 0;
	const char* args_doc;
	const char* execname;

	// Get program name from parser or use generic program if NULL
	execname = parser->execname;
	if (!execname)
		execname = "PROGRAM";

	// Get synopsis lines from parser or use a generic one if NULL
	args_doc = parser->args_doc;
	if (!args_doc)
		args_doc = "[options] args...";

	// For each line in args_doc, print a usage line
	while (args_doc[0] != '\0') {
		// Get length of next argument list doc (terminated by \n)
		len = get_first_token_length(args_doc, '\n');
		fprintf(stream, "  %s %.*s\n", execname, len, args_doc);

		// Skip '\n' character
		while (args_doc[len] == '\n')
			len++;

		// Update to point to the next synopsis line
		args_doc += len;
	}
}


/**
 * set_option_synopsis() - format an option synopsis string
 * @synopsis:   buffer receiving the option string
 * @opt:        option parser
 * @valname:    the value name as it must appear in the synopsis. If NULL
 *              and if option accepts value, the string "VALUE" is used
 *              instead.
 *
 * Return: length of synopsis (excluding null termination character)
 */
static
int set_option_synopsis(char* synopsis, const struct mm_arg_opt* opt,
                        const char* valname)
{
	char* str = synopsis;
	int key = mm_arg_opt_get_key(opt);
	const char* name = mm_arg_opt_get_name(opt);
	int reqflags = opt->flags & MM_OPT_REQMASK;

	// Insert small indentation
	*str++ = ' ';
	*str++ = ' ';

	// Add short option name synopsis
	if (key) {
		if (reqflags == MM_OPT_NOVAL)
			str += sprintf(str, "-%c", key);
		else if (reqflags == MM_OPT_NEEDVAL)
			str += sprintf(str, "-%c %s", key, valname);
		else
			str += sprintf(str, "-%c [%s]", key, valname);
	}

	// Add separator if short and long name are provided
	if (key && name) {
		*str++ = ',';
		*str++ = ' ';
	}

	// Add long name option synopsis
	if (name) {
		if (reqflags == MM_OPT_NOVAL)
			str += sprintf(str, "--%s", name);
		else if (reqflags == MM_OPT_NEEDVAL)
			str += sprintf(str, "--%s=%s", name, valname);
		else
			str += sprintf(str, "--%s[=%s]", name, valname);
	}

	// return the number of characters written in synopsis
	return str - synopsis;
}


/**
 * print_option() - print option doc on stream
 * @opt:        option whose documentation must be printed
 * @stream:     output stream on which the option doc must be written
 */
static
void print_option(const struct mm_arg_opt* opt, FILE* stream)
{
	char synopsis[OPT_SYNOPSIS_MAXLEN];
	char type_doc[VALUE_NAME_MAXLEN+64];
	char value_name[VALUE_NAME_MAXLEN] = "VALUE";
	const char * opt_desc;
	char* desc;
	int len, desc_len;
	int type = mm_arg_opt_get_type(opt);
	bool is_positive;

	opt_desc = opt->desc ? opt->desc : "";
	desc_len = strlen(opt_desc);
	desc = mm_malloca(desc_len + sizeof(type_doc));
	copy_opt_desc(desc, opt_desc, value_name);

	// Set option synopsis
	len = set_option_synopsis(synopsis, opt, value_name);

	// Write synopsis on its own line if too long
	if (len >= OPT_INDENT_LEN) {
		fprintf(stream, "%s\n", synopsis);
		synopsis[0] = '\0';
	}

	// Append value type specification if not string type
	if (type != MM_OPT_STR) {
		is_positive = (type == MM_OPT_UINT || type == MM_OPT_ULLONG);
		sprintf(type_doc, "%s%s must be a%s integer.",
		        desc_len ? " " : "", value_name,
		        is_positive ? " non negative" : "n");
		strcat(desc, type_doc);
	}

	print_text_wrapped(LINE_MAXLENGTH, desc,
	                   OPT_INDENT_LEN, synopsis, stream);

	mm_freea(desc);
}


/**
 * print_help() - print usage help on stream
 * @parser:     argument parser whose help must be printed
 * @stream:     output stream on which usage must be written
 */
static
void print_help(const struct mm_arg_parser* parser, FILE* stream)
{
	int i;
	const char* doc = parser->doc;

	fprintf(stream, "Usage:\n");
	print_synopsis(parser, stream);

	if (doc) {
		fputc('\n', stream);
		print_text_wrapped(LINE_MAXLENGTH, doc, 0, NULL, stream);
		fputc('\n', stream);
	}

	fprintf(stream, "\nOptions:\n");

	for (i = 0; i < parser->num_opt; i++)
		print_option(&parser->optv[i], stream);

	print_option(&help_opt, stream);
}


/*************************************************************************
 *                                                                       *
 *                     Completion helper functions                       *
 *                                                                       *
 *************************************************************************/

static
int is_completing(const struct mm_arg_parser* parser)
{
	return parser->flags & MM_ARG_PARSER_COMPLETION;
}


/**
 * complete_longopt() - print long option completion candidate
 * @opt:        pointer to opt
 * @len:        length of str_start
 * @name_start: beginning of option name supplied on cmdline
 *
 * If beginning of long option name of @opt match @str_start, then print on
 * standard output the synopsis of the long option form of @opt.
 */
static
void complete_longopt(const struct mm_arg_opt* opt,
                      int len, const char* name_start)
{
	const char* lname = mm_arg_opt_get_name(opt);

	// Filter completion candidate: the supplied argument must match the
	// beginning of option name
	if (!lname || strncmp(name_start, lname, len))
		return;

	switch (opt->flags & MM_OPT_REQMASK) {
	case MM_OPT_NOVAL:
		printf("--%s\n", lname);
		break;

	case MM_OPT_NEEDVAL:
		printf("--%s=\n", lname);
		break;

	case MM_OPT_OPTVAL:
		printf("--%s=\n", lname);
		printf("--%s\n", lname);
		break;
	}
}


/**
 * complete_longopts() - generate list of long option for completion
 * @parser:     argument parser configuration
 * @arg:        beginning of argument supplied on cmdline that should be
 *              completed. To be passed without leading "--" (can be NULL)
 *
 * Print on standard output the list long options compatible with the partial
 * option name specified by @arg. If @arg is NULL, it is assumed to
 * be empty.
 *
 * Return: always MM_ARGPARSE_COMPLETE
 */
static
int complete_longopts(const struct mm_arg_parser* parser, const char* arg)
{
	int i, len;

	if (arg == NULL)
		arg = "";

	len = strlen(arg);

	for (i = 0; i < parser->num_opt; i++)
		complete_longopt(&parser->optv[i], len, arg);

	complete_longopt(&help_opt, len, arg);

	return MM_ARGPARSE_COMPLETE;
}


/**
 * complete_shortopts() - generate list of short option for completion
 * @arg:        beginning of argument supplied on cmdline that should be
 *              completed. To be passed without leading "-" (can be NULL)
 * @parser:     argument parser configuration
 *
 * Return: always MM_ARGPARSE_COMPLETE
 */
static
int complete_shortopts(const struct mm_arg_parser* parser, const char* arg)
{
	int i, key, len;

	if (arg == NULL)
		arg = "";

	len = strlen(arg);

	// Verify that all previous short option in the argument are
	// recognized. If not, do not propose any completion.
	for (i = 0; i < len; i++) {
		if (!find_opt(parser, arg[i], NULL, 0))
			return MM_ARGPARSE_COMPLETE;
	}

	// If there is already options in argument, propose only the current
	// argument as completion, this will make completion add space and
	// move to a new argument
	if (len != 0) {
		printf("-%s\n", arg);
		return MM_ARGPARSE_COMPLETE;
	}

	// We have currently an empty argument, loop over all option
	// providing short key display tham as completion proposal
	for (i = 0; i < parser->num_opt; i++) {
		key = mm_arg_opt_get_key(&parser->optv[i]);
		if (key)
			printf("-%c\n", key);
	}

	printf("-h\n");
	return MM_ARGPARSE_COMPLETE;
}


/**
 * complete_opt_value() - gen list of option value acceptable for option
 * @parser:     argument parser configuration
 * @opt:        option whose value is being completed
 * @arg:        beginning of value supplied on cmdline
 *
 * Return: always MM_ARGPARSE_COMPLETE
 */
static
int complete_opt_value(const struct mm_arg_parser* parser,
                       const struct mm_arg_opt* opt, const char* arg)
{
	int rv, flags;
	union mm_arg_val value = {.str = arg};

	if (parser->cb) {
		rv = parser->cb(opt, value, parser->cb_data,
		                MM_ARG_OPT_COMPLETION);
		if (rv)
			return MM_ARGPARSE_COMPLETE;
	}

	if (opt->flags & (MM_OPT_FILEPATH | MM_OPT_DIRPATH)) {
		flags = 0;
		if (opt->flags & MM_OPT_FILEPATH)
			flags |= MM_DT_REG | MM_DT_DIR;

		if (opt->flags & MM_OPT_DIRPATH)
			flags |= MM_DT_DIR;

		mm_arg_complete_path(arg, flags, NULL, NULL);
	} else if (arg) {
		printf("%s\n", arg);
	}

	return MM_ARGPARSE_COMPLETE;
}


/*************************************************************************
 *                                                                       *
 *                    Argument processing functions                      *
 *                                                                       *
 *************************************************************************/

/**
 * match_opt_key_or_name() - test whether a name or key match an option
 * @opt:        option to try
 * @key:        key to test against @opt. If @key is not set to IGNORE_KEY,
 *              the matching is done based on short option key. Otherwise
 *              the matching is done based on long name option (using @name
 *              and @namelen)
 * @name:       string of name to test against long name of @opt. This
 *              string does NOT need to be null terminated
 * @namelen:    length of @name string
 *
 * Return: true if supplied key or name match @opt, false otherwise
 */
static
bool match_opt_key_or_name(const struct mm_arg_opt* opt,
                           int key, const char* name, int namelen)
{
	const char* opt_name;

	// If key is not IGNORE_KEY, the matching MUST be done based on key
	if (key != IGNORE_KEY)
		return (key == mm_arg_opt_get_key(opt));

	// Do matching based on long since key is IGNORE_KEY
	opt_name = mm_arg_opt_get_name(opt);
	if (opt_name != NULL
	    && !strncmp(opt_name, name, namelen)
	    && opt_name[namelen] == '\0')
		return true;

	return false;
}


/**
 * find_opt() - find the option that match supplied key or name
 * @parser:     argument parser
 * @key:        key to test against the different option of @parser.
 *              matching is based on key or name depending on whether @key
 *              is set to IGNORE_KEY or not, see match_opt_key_or_name().
 * @name:       string of name to test against long name options. This
 *              string does NOT need to be null terminated
 * @namelen:    length of @name string
 *
 * Search among the options of @parser to find a matching one. If @key is
 * 'h' or @name is "help", this function will print the usage on standard
 * error and exit.
 *
 * Return: pointer to option if found among the configured options of
 * @parser. NULL if option is not found. Please note that is @key or @name
 * appears to be a request to display help, the function does not return.
 */
static
const struct mm_arg_opt* find_opt(const struct mm_arg_parser* parser,
                                  int key, const char* name, int namelen)
{
	int i, num_opt;
	const struct mm_arg_opt* opt;

	// Test in user provided options
	num_opt = parser->num_opt;
	opt = parser->optv;
	for (i = 0; i < num_opt; i++, opt++) {
		if (match_opt_key_or_name(opt, key, name, namelen))
			return opt;
	}

	// Check option is not help
	if (match_opt_key_or_name(&help_opt, key, name, namelen)) {
		return &help_opt;
	}

	return NULL;
}


/**
 * print_opt_error() - print message error related to an option
 * @opt:        option relative to which the error has been detected
 * @msg:        string containing the message.
 *
 * Print a error message on standard error by prefixing the option name
 * before displaying @msg.
 */
static
void print_opt_error(const struct mm_arg_opt* opt, const char* msg, ...)
{
	va_list args;
	const char* name = mm_arg_opt_get_name(opt);
	int key = mm_arg_opt_get_key(opt);

	if (name && key)
		fprintf(stderr, "Option -%c|--%s ", key, name);
	else if (name)
		fprintf(stderr, "Option --%s ", name);
	else
		fprintf(stderr, "Option -%c ", key);

	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);

	fputc('\n', stderr);
}


/**
 * cast_ll_to_argval() - copy long long value into proper field of argval
 * @opt:        option parser specifying which cast to perform
 * @argval:     pointer to argval union that will receive the result
 * @llval:      long long value to cast
 *
 * Return: 0 in case of success, -1 otherwise (cast problem)
 */
static
int cast_ll_to_argval(const struct mm_arg_opt* opt,
                      union mm_arg_val * argval, long long llval)
{
	int type = mm_arg_opt_get_type(opt);

	switch (type) {
	case MM_OPT_LLONG:
		argval->ll = llval;
		break;

	case MM_OPT_INT:
		if (llval < INT_MIN || llval > INT_MAX)
			goto error;

		argval->i = llval;
		break;

	case MM_OPT_UINT:
		if (llval < 0 || llval > UINT_MAX)
			goto error;

		argval->ui = llval;
		break;

	default:
		errno = EINVAL;
		return -1;
	}

	return 0;

error:
	errno = ERANGE;
	return -1;
}


/**
 * check_value_is_positive() - verify string is not negative integer
 * @str:        string for value
 *
 * Return: 0 if @str does not represent a negative value, -1 otherwise with
 * errno set to ERANGE.
 */
static
int check_value_is_positive(const char* str)
{
	// Skip whitespace
	while (isspace(*str))
		str++;

	// assert leading non whitespace character is not minus
	if (*str == '-') {
		errno = ERANGE;
		return -1;
	}

	return 0;
}


/**
 * conv_str_to_argval() - convert string into suitable type for option
 * @opt:        option specifying which conversion must be performed.
 * @argval:     pointer to argval union that will receive the result
 * @value:      string of the value to convert
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int conv_str_to_argval(const struct mm_arg_opt* opt,
                       union mm_arg_val * argval, const char* value)
{
	int type = mm_arg_opt_get_type(opt);
	long long llval = 0;  // shut up false warning about uninitialized
	                      // llval when type == MM_OPT_ULLONG
	char* endptr;
	const char* valtype;
	int prev_err = errno;

	if (type == MM_OPT_STR) {
		argval->str = value;
		return 0;
	}

	// Value type is not string, hence it requires a conversion. So now
	// ensure that value is not empty
	if (!value || value[0] == '\0') {
		errno = EINVAL;
		goto error;
	}

	// Convert all strings as long long excepting if requesting
	// ulonglong
	errno = 0;
	if (type == MM_OPT_ULLONG) {
		// Prevent to convert negative value (strtoull() would
		// accept them)
		if (check_value_is_positive(value)) {
			errno = ERANGE;
			goto error;
		}

		argval->ull = strtoull(value, &endptr, 0);
	} else {
		llval = strtoll(value, &endptr, 0);
	}

	// Check the whole string has been used for conversion
	if (*endptr != '\0') {
		errno = EINVAL;
		goto error;
	}

	if (errno != 0)
		goto error;

	// If not requesting ulonglong, do final conversion of longlong to
	// requested type
	if (type != MM_OPT_ULLONG
	    && cast_ll_to_argval(opt, argval, llval))
		goto error;

	errno = prev_err;
	return 0;

error:
	valtype = get_value_type_name(type);
	print_opt_error(opt, "accepting %s value type has received an "
	                "invalid value \"%s\" (%s)",
	                valtype, value, strerror(errno));
	return -1;
}

/**
 * mm_arg_opt_set_value() - set value to supplied pointer
 * @opt:      option whose value has to be set
 * @val:      value to set. Can be NULL if case of string type option.
 *
 * Set value if @opt->strptr (or any aliased pointer) is not NULL. In such
 * case, the conversion is performed (if needed) according to the type
 * specified in @opt->flags.
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int mm_arg_opt_set_value(const struct mm_arg_opt* opt, union mm_arg_val val)
{
	int type = mm_arg_opt_get_type(opt);

	// Ignore setting value if no pointer has been set
	if (!opt->val.sptr)
		return 0;

	switch (type) {
	case MM_OPT_STR: *opt->val.sptr = val.str; break;
	case MM_OPT_INT: *opt->val.iptr = val.i; break;
	case MM_OPT_UINT: *opt->val.uiptr = val.ui; break;
	case MM_OPT_LLONG: *opt->val.llptr = val.ll; break;
	case MM_OPT_ULLONG: *opt->val.ullptr = val.ull; break;
	default:
		print_opt_error(opt, "has unknown value type");
		return -1;
	}

	return 0;
}


/**
 * process_opt_value() - process argument value
 * @opt:        matched option
 * @value:      string of value if one has been supplied, NULL otherwise
 * @parser:     parser used
 *
 * Return: 0 in case of success, or MM_ARGPARSE_ERROR (-1) if a validation issue
 * has occurred and MM_ARGPARSE_STOP (-2) if early stop has been requested.
 */
static
int process_opt_value(const struct mm_arg_opt* opt, const char* value,
                      const struct mm_arg_parser* parser)
{
	void* cb_data = parser->cb_data;
	mm_arg_callback cb = parser->cb;
	int reqflags = opt->flags & MM_OPT_REQMASK;
	union mm_arg_val argval;
	int rv;

	// If the recognized option is the help option added internally, just
	// print help and return stop
	if (opt == &help_opt) {
		if (is_completing(parser))
			return 0;

		print_help(parser, stdout);
		return MM_ARGPARSE_STOP;
	}

	if ((reqflags == MM_OPT_NOVAL) && value) {
		print_opt_error(opt, "does not accept any value.");
		return MM_ARGPARSE_ERROR;
	}

	if ((reqflags == MM_OPT_NEEDVAL) && !value) {
		print_opt_error(opt, "needs value.");
		return MM_ARGPARSE_ERROR;
	}

	if (!value)
		value = opt->defval;

	// Convert string value to argval union and run callback if one is
	// present
	if ((rv = conv_str_to_argval(opt, &argval, value)) < 0
	    || (cb && (rv = cb(opt, argval, cb_data, 0)) < 0))
		return rv;

	// set value if specified in option parser
	return mm_arg_opt_set_value(opt, argval);
}


/**
 * process_short_opt() - find and parses option assuming short key
 * @parser:     argument parser to use
 * @opts:       string of concatenated options (without -)
 * @next_arg:   pointer to next argument if present, NULL otherwise
 * @next_is_last: set to one if @next_arg is the last argument of cmdline
 *
 * Return: a non-negative number indicating the number of next argument to
 * skip in case of success. -1 in case of failure
 */
static
int process_short_opt(const struct mm_arg_parser* parser,
                      const char* opts, const char* next_arg, int next_is_last)
{
	const struct mm_arg_opt* opt_parser;
	const char* value = NULL;
	int move_arg_index = 0;
	int rv, reqflags;

	if (is_completing(parser) && !next_arg)
		return complete_shortopts(parser, opts);

	while (opts[0] != '\0') {
		opt_parser = find_opt(parser, opts[0], NULL, 0);
		if (!opt_parser) {
			fprintf(stderr, "Unsupported option -%c\n", opts[0]);
			return -1;
		}

		// It is allowed to interpret the next argument as value
		// only if the option key is the last one of the list
		reqflags = opt_parser->flags & MM_OPT_REQMASK;
		if ((reqflags != MM_OPT_NOVAL)
		    && (opts[1] == '\0')
		    && !is_arg_an_option(next_arg)) {
			value = next_arg;
			move_arg_index = 1;
			if (is_completing(parser) && next_is_last)
				return complete_opt_value(parser,
				                          opt_parser, value);
		}

		rv = process_opt_value(opt_parser, value, parser);
		if (rv < 0)
			return rv;

		opts++;
	}

	// Advance in argument list if value was
	return move_arg_index;
}


/**
 * process_long_opt() - find and parses option assuming long option
 * @parser:     argument parser to use
 * @arg:        argument supplied (without --)
 * @do_complete: execute completion
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int process_long_opt(const struct mm_arg_parser* parser, const char* arg,
                     int do_complete)
{
	const char* name;
	const char* value;
	int namelen, rv;
	const struct mm_arg_opt* opt;

	// Set the name and value token
	name = arg;
	namelen = get_first_token_length(arg, '=');
	value = (arg[namelen] == '=') ? arg+namelen+1 : NULL;

	// If we are in the mode of autocompletion mode, generate
	// the list of long options if we are still not writing value
	if (do_complete && arg[namelen] != '=') {
		complete_longopts(parser, arg);
		return MM_ARGPARSE_COMPLETE;
	}

	// Search for a matching option
	opt = find_opt(parser, IGNORE_KEY, name, namelen);
	if (!opt) {
		if (do_complete)
			return MM_ARGPARSE_COMPLETE;

		fprintf(stderr, "Unsupported option --%.*s\n", namelen, arg);
		return MM_ARGPARSE_ERROR;
	}

	if (do_complete)
		return complete_opt_value(parser, opt, value);

	// Process the found option
	if ((rv = process_opt_value(opt, value, parser)) < 0)
		return rv;

	return 0;
}


/**
 * validate_options() - validate options settings
 * @parser:     argument parser to validate
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int validate_options(const struct mm_arg_parser* parser)
{
	int i, key;
	const char* lname;
	const struct mm_arg_opt* opt;

	for (i = 0; i < parser->num_opt; i++) {
		opt = &parser->optv[i];

		// Ensure the name field is set
		if (!opt->name || opt->name[0] == '\0') {
			fprintf(stderr, "name in mm_arg_opt must be set\n");
			return -1;
		}

		key = mm_arg_opt_get_key(opt);
		lname = mm_arg_opt_get_name(opt);

		// Validate option key and/or long option name
		if ((key && !is_valid_short_opt_key(key))
		    || (lname && !is_valid_long_opt_name(lname, false))) {
			fprintf(stderr, "invalid short or long name for "
			        "option %s\n", opt->name);
			return -1;
		}
	}

	return 0;
}


/**
 * early_stop_parsing() - Stop argument parsing
 * @parser:     argument parser configuration
 * @retval:     code returned indicating why parsing is interrupted
 *
 * This function exits program if MM_ARG_PARSER_NOEXIT is not set in
 * @parser->flags. If the parsing has been interrupted because of parsing error
 * (@retval == -1), a small reminder how to use help is reported to stderr.
 *
 * Return: @retval if MM_ARG_PARSER_NOEXIT is set in @parser->flags (otherwise,
 * the function call exit)
 */
static
int early_stop_parsing(const struct mm_arg_parser* parser, int retval)
{
	int exitcode = EXIT_SUCCESS;

	if (retval == MM_ARGPARSE_ERROR) {
		fprintf(stderr, "Use -h or --help to display usage.\n");
		exitcode = EXIT_FAILURE;
	}

	if (parser->flags & MM_ARG_PARSER_NOEXIT)
		return retval;

	exit(exitcode);
}


/**
 * mm_arg_parse() - parse command-line options
 * @parser:     argument parser configuration
 * @argc:       argument count as passed to main()
 * @argv:       argument array as passed to main()
 *
 * This functions parses the arguments in argv, of length argc, as provided
 * by the argument of main() using the configuration set in @parser. The
 * supported options are specified in the @parser->optv array. Even if it is
 * not set in @parser->optv, a parser always supports the "-h" or "--help"
 * option. If encountered, the program usage will be printed on standard
 * output and the process will exit with EXIT_SUCCESS code (This behaviour
 * can be overridden if "h|help" is explicitly defined as option in
 * @parser->optv). If the parsing operation fails (because of invalid option
 * or value), the error diagnostic will be printed on standard error and the
 * process will exit with EXIT_FAILURE code. In other case, the parsing will
 * continued until "--" or a non optional argument is encountered.
 *
 * The value of @parser->flags is a OR-combination of any number of the
 * following flags :
 *
 * - %MM_ARG_PARSER_NOEXIT: the process will not exit in case of help printing
 *   nor in case of error but mm_arg_parse() will return respectively
 *   MM_ARGPARSE_STOP and MM_ARGPARSE_ERROR.
 * - %MM_ARG_PARSER_COMPLETION: the parser is invoked for being used in shell
 *   completion script. In case of unknown option, the completed candidate
 *   options are printed on standard output (in a format suitable for bash
 *   completion script).
 *
 * There are 2 non-exclusive ways to get the values of the option supplied
 * on command line
 *
 * #. setting the struct mm_arg_opt->*ptr field to the data that must be set
 *    when an option is found and parsed.
 * #. using the callback function @parser->cb and data @parser->cb_data.
 *
 * Return: a non negative value indicating the index of the first non-option
 * argument when argument parsing has been successfully finished. Additionally
 * if MM_ARG_PARSER_NOEXIT is set in @parser->flags :
 *
 * - MM_ARGPARSE_ERROR (-1): an error of argument parsing or validation occurred
 * - MM_ARGPARSE_STOP (-2): help display has been requested or early parsing
 *   stop has been requested by callback.
 * - MM_ARGPARSE_COMPLETE (-3): parser was in completion mode and the last
 *   argument has been completed (completion candidates have been printed on
 *   output).
 */
API_EXPORTED
int mm_arg_parse(const struct mm_arg_parser* parser, int argc, char* argv[])
{
	const char * arg, * next_arg;
	int index, r, do_complete;

	if (validate_options(parser))
		return early_stop_parsing(parser, MM_ARGPARSE_ERROR);

	do_complete = 0;
	for (index = 1; index < argc; index++) {
		arg = argv[index];

		// If opt has not the format of option, we must stop
		// processing options
		if (arg[0] != '-')
			break;

		// if arg has form "-string", process as short option
		if (is_valid_short_opt_key(arg[1])) {
			next_arg = (index+1 < argc) ? argv[index+1] : NULL;
			r = process_short_opt(parser, arg+1, next_arg,
			                      index+2 == argc);
			if (r < 0)
				return early_stop_parsing(parser, r);

			index += r;
			continue;
		}

		// If completion is enabled, run it on last argument
		if ((index == argc-1) && is_completing(parser))
			do_complete = 1;

		// Complete if we have an incomplete option
		if ((arg[1] == '\0') && do_complete) {
			mm_arg_parse_complete(parser, arg);
			return early_stop_parsing(parser, MM_ARGPARSE_COMPLETE);
		}

		if (arg[1] != '-')
			break;

		// if arg is "--", position the argument index to the next
		// argument and stop processing options
		if ((arg[2] == '\0') && !do_complete)
			return index+1;

		// arg has the form of "--string", process as long option
		if ((r = process_long_opt(parser, arg+2, do_complete)) < 0)
			return early_stop_parsing(parser, r);
	}

	return index > argc ? argc : index;
}

/**
 * mm_arg_optv_parse() - parse command-line options
 * @optn:       number of mm_arg_opt elements in optv
 * @optv:       pointer to mm_arg_opt array
 * @argc:       argument count as passed to main()
 * @argv:       argument array as passed to main()
 *
 * This function wraps around mm_arg_parse and it takes care to create and
 * initialize a minimal mm_arg_parser structure.
 * It can be useful when no extended usage documentation is needed, as it
 * just provides the standard help function.
 *
 * Return: a non negative value indicating the index of the first non-option
 * argument when argument parsing has been successfully finished.
 */
API_EXPORTED
int mm_arg_optv_parse(int optn, const struct mm_arg_opt* optv, int argc,
                      char* argv[])
{
	struct mm_arg_parser parser = {
		.optv = optv,
		.num_opt = optn > 0 ? optn : 0,
		.execname = argv[0]
	};
	return mm_arg_parse(&parser, argc, argv);
}

/**
 * mm_arg_parse_complete() - print list of opts of arg parser for completion
 * @parser:     argument parser configuration
 * @arg:        beginning of argument (can be NULL)
 *
 * This function print on standard output the list of option accepted by
 * parser and whose beginning match @arg (if supplied). This list is the
 * same format as bash compgen command.
 *
 * Return: 0 in case of success, MM_ARGPARSE_ERROR otherwise (validation
 * error occurred)
 */
API_EXPORTED
int mm_arg_parse_complete(const struct mm_arg_parser* parser, const char* arg)
{
	int len;

	if (validate_options(parser))
		return early_stop_parsing(parser, MM_ARGPARSE_ERROR);

	if (!arg)
		return 0;

	len = strlen(arg);

	// Short option completion is triggered by "" and "-something"
	if (len < 1 || arg[0] == '-')
		complete_shortopts(parser, len >= 1 ? arg+1 : "");

	// Long option completion is triggered by "", "-", "--something"
	if ((len == 0)
	    || (len == 1 && arg[0] == '-')
	    || (len >= 2 && arg[0] == '-' && arg[1] == '-'))
		complete_longopts(parser, len >= 2 ? arg+2 : "");

	return 0;
}


/**
 * mm_arg_complete_path() - complete argument as path
 * @arg:        beginning of argument (must not be NULL)
 * @type_mask:  Combination of MM_DT_* flags indicating the desired file
 * @cb:         user supplied completion callback (can be NULL)
 * @cb_data:    pointer passed to @cb if it is not NULL
 *
 * This function print on standard output the list of path whose beginning
 * match @arg and whose type match the mask specified by type_mask. This
 * list has the same format as bash compgen command.
 *
 * More path candidates can be filtered out by supplying a callback through
 * the @cb argument. If not NULL, this function will be called for each
 * completion candidate which will be discarded if the callback does not
 * return 1.
 *
 * Return: 0 in case of success, -1 otherwise
 */
API_EXPORTED
int mm_arg_complete_path(const char* arg, int type_mask,
                         mm_arg_complete_path_cb cb, void* cb_data)
{
	MM_DIR* dir;
	const struct mm_dirent* dirent;
	char * dirpath, * base;
	const char * disp_dir, * name;
	int i, arglen, baselen, type, isdir;
	int rv = -1;

	if (arg == NULL)
		return mm_raise_error(EINVAL, "arg pointer must not be NULL");

	arglen = strlen(arg);
	base = mm_malloca(arglen + 1);
	dirpath = mm_malloca(arglen + 2);
	if (!base || !dirpath)
		goto exit;

	// If arg is written as a folder, start search from it, otherwise,
	// we compute its parent dir and use the basename as filter
	if (arglen == 0 || is_path_separator(arg[arglen-1])) {
		strcpy(dirpath, arglen ? arg : "./");
		strcpy(base, "");
		baselen = 0;
	} else {
		i = mm_dirname(dirpath, arg);
		dirpath[i++] = '/';
		dirpath[i] = '\0';
		baselen = mm_basename(base, arg);
	}

	// If argument has no separator, don't prepend "./" artificially to
	// completion candidates.
	disp_dir = "";
	for (i = 0; i < arglen; i++) {
		if (is_path_separator(arg[i]))
			disp_dir = dirpath;
	}

	dir = mm_opendir(dirpath);
	if (!dir)
		goto exit;

	while (1) {
		dirent = mm_readdir(dir, NULL);
		if (!dirent)
			break;

		name = dirent->name;
		type = dirent->type;

		// Discard if base does not match the beginning of filename
		if (strncmp(name, base, baselen) != 0
		    || is_wildcard_directory(name))
			continue;

		// Discard if a callback did not return 1
		if (cb && cb(name, dirpath, type, cb_data) != 1)
			continue;

		// If type does not match expected, just discard
		if (!(type & type_mask))
			continue;

		// Display candidate. If it a dir, append a "/"
		isdir = (type & MM_DT_DIR);
		printf("%s%s%s\n", disp_dir, name, isdir ? "/" : "");
	}

	mm_closedir(dir);
	rv = 0;

exit:
	mm_freea(dirpath);
	mm_freea(base);
	return rv;
}


/**
 * mm_arg_is_completing() - indicates whether shell completion is running
 *
 * The function indicates if command completion has been requested through
 * %MMLIB_CMD_COMPLETION environment variable: by convention command
 * completion is requested if this variable is set (no matter the value).
 *
 * Please note that you are not forced to use this environment variable to
 * trigger command completion. You may use any environment variable or any
 * other mechanism of your choosing. If mm_arg_parse() is used to parse
 * options, completion will run only if the flag %MM_ARG_PARSER_COMPLETION
 * is set in the flags field of struct mm_arg_parser. However on the other
 * hand, there is little reason not to use %MMLIB_CMD_COMPLETION environment
 * variable. Hence if a code is mm_arg_parse() and using command completion
 * through its executable, it is invited to set %MM_ARG_PARSER_COMPLETION if
 * mm_arg_is_completing() returns 1.
 *
 * Return: 1 if completion is running, 0 otherwise.
 */
API_EXPORTED
int mm_arg_is_completing(void)
{
	static int is_completing = -1;

	if (is_completing >= 0)
		return is_completing;

	is_completing = mm_getenv("MMLIB_CMD_COMPLETION", NULL) ? 1 : 0;
	return is_completing;
}
