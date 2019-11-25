/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <windows.h>
#include <stdbool.h>
#include <uchar.h>

#include "mmlib.h"
#include "utils-win32.h"


/**************************************************************************
 *                                                                        *
 *           Command line conversion in UTF-8 argument array              *
 *                                                                        *
 **************************************************************************/

/**
 * alloc_argv_block() - allocate a memory block suitable for argv
 * @p_args:     pointer to pointer variable that will receive the buffer
 *              used to hold all the argument strings
 * @cmdline:    commandline in UTF-8
 *
 * This function scan roughly the commandline passed in @cmdline argument
 * and estimate the maximum number of arguments that command line contains
 * and the maximum size needed to hold all the argument strings. Based on
 * this, it allocates a memory block that can hold the argument array
 * pointer and a buffer for the argument strings. In case of success, the
 * buffer of argument string is set in *@p_args.
 *
 * Return: a memory block large enough to the array of pointer and a buffer
 * holding the string of the arguments. Both element are contiguous but the
 * memory starts with the array of pointer hence the type returned. In case
 * of failure (allocation problem), NULL is returned.
 */
static
char** alloc_argv_block(char** p_args, const char* cmdline)
{
	int maxnarg, i;
	size_t maxsize;
	void* ptr;
	char* args;

	// First argument (executable path) count for one
	maxnarg = 1;

	// Search for length of commandline and count number of space
	// character. There are at most as number of argument as the number of
	// spaces
	for (i = 0; cmdline[i]; i++) {
		if (cmdline[i] == ' ')
			maxnarg++;
	}

	// Allocate the memory block (both pointer array and string pool
	// contiguous)
	maxsize = (i+1) + (maxnarg+1)*sizeof(char*);
	ptr = malloc(maxsize);
	if (!ptr)
		return NULL;

	// String pool starts right after the array of pointer
	args = ptr;
	args += (maxnarg+1)*sizeof(char*);
	*p_args = args;

	return ptr;

}


/**
 * write_nbackslash() - write a specified number of backslash in string
 * @str:        buffer to which the backslash must be written
 * @nbackslash: number of backslash character that must be written
 *
 * Return: the char pointer immediately after the @nbackslash have been
 * written, ie, @str + @nbackslash.
 */
static
char* write_nbackslash(char* str, int nbackslash)
{
	for (; nbackslash; nbackslash--)
		*str++ = '\\';

	return str;
}


/**
 * parse_cmdline() - convert command line into array of argument string
 * @p_argv:     pointer to an argument array variable that will receive the
 *              allocated null terminated array of argument strings
 * cmdline:     string of command line
 *
 * This function convert the command line into a NULL terminated array of
 * argument string. The argument will be transformed following the
 * convention in:
 * https://msdn.microsoft.com/en-us/library/windows/desktop/bb776391(v=vs.85).aspx
 *
 * Return: in case of success, the number of argument in the array of
 * argument strings. In such a case, *@p_argv receive the array. In case of
 * failure, -1 is returned.
 */
static
int parse_cmdline(char*** p_argv, const char* cmdline)
{
	char c, *str, **argv;
	int argc = 0, nbackslash = 0;
	bool in_quotes = false, prev_is_whitespace = false;

	argv = alloc_argv_block(&str, cmdline);
	if (!argv)
		return -1;

	argv[0] = str;

	// Loop over all character in cmdline (until null termination)
	while ( (c = *cmdline++) != '\0') {
		switch (c) {
		case '\\':
			nbackslash++;
			prev_is_whitespace = false;
			break;

		case '"':
			if (nbackslash % 2 == 0) {
				str = write_nbackslash(str, nbackslash/2);
				in_quotes = !in_quotes;
			} else {
				str = write_nbackslash(str, (nbackslash-1)/2);
				*str++ = '"';
			}
			nbackslash = 0;
			prev_is_whitespace = false;
			break;

		case '\t':
		case '\r':
		case '\n':
		case '\v':
		case '\f':
		case ' ':
			// If not in quotes, terminate argument string,
			// otherwise normal processing
			if (!in_quotes) {
				if (prev_is_whitespace)
					break;

				*str++ = '\0';
				argv[++argc] = str;
				prev_is_whitespace = true;
				break;
			}
			/* fall through */

		default:
			str = write_nbackslash(str, nbackslash);
			nbackslash = 0;
			*str++ = c;
			prev_is_whitespace = false;
			break;
		}
	}

	// Terminate current argument string and the argv pointer array
	if (!prev_is_whitespace) {
		*str++ = '\0';
		argc++;
	}
	argv[argc] = NULL;

	*p_argv = argv;
	return argc;
}


/**
 * setup_argv_utf8() - allocate and set argument array in UTF-8
 * @p_argv:     pointer to a variable receiving the resulting argument array
 *
 * This function allocate and set a NULL terminated array of arguments in UTF-8.
 *
 * Return: an non negative number of argument in *@p_argv in case of success,
 * -1 otherwise.
 */
static
int setup_argv_utf8(char*** p_argv)
{
	int cmdline_u8_len, retval;
	char* cmdline_u8;
	char16_t* cmdline_u16;

	// Get UTF-16 command line from WIN32 API
	cmdline_u16 = GetCommandLineW();

	// Estimate the size of the command line converted in UTF-8
	cmdline_u8_len = get_utf8_buffer_len_from_utf16(cmdline_u16);
	if (cmdline_u8_len < 0)
		return -1;

	// Allocate UTF-8 string on stack (or heap if too big)
	cmdline_u8 = mm_malloca(cmdline_u8_len * sizeof(*cmdline_u8));
	if (!cmdline_u8)
		return -1;

	// Convert the UTF-16 command line into a stack allocated UTF-8 string
	conv_utf16_to_utf8(cmdline_u8, cmdline_u8_len, cmdline_u16);

	// Do the actual split command line -> argument array
	retval = parse_cmdline(p_argv, cmdline_u8);

	mm_freea(cmdline_u8);
	return retval;
}


/**************************************************************************
 *                                                                        *
 *                 Override of UCRT/MSVCRT argv setup                     *
 *                                                                        *
 **************************************************************************/

int __getmainargs(int * p_argc, char *** p_argv, char *** p_env,
                  int do_wildcard,  void* startinfo);

int _get_startup_argv_mode(void);

/**
 * DOC: Supporting UTF-8 argument in main() in Windows
 *
 * Rationale:
 * Windows support unicode natively, however only through UTF-16 which is the
 * native format of string in NT kernel. When a *A() variant of an WINAPI
 * function is called, the Win32 subsystem transforms the char* string into
 * UTF-16 (with the WHAR type) using the active codepage (accessible through
 * GetACP()) and makes a kernel syscall. Now while it exists a UTF-8 codepage
 * (CP_UTF8, 65001), it cannot be set as active codepage. If attempted, this
 * provokes various crashes making the system unusable (I've heard someone had
 * to revert back to a previous restore point). This codepage can only be used
 * as argument of MultiBytetoWideChar() and WideCharToMultiByte() functions.
 * Since this issue has been known for more than 10-15 years and Microsoft has
 * never shown any will to fix it, we must assume this will remain for the
 * lifetime of Windows.
 *
 * For this reason, if we want to reasonably support UTF-8 as argument array
 * when spawning a new process or in a new executable (ie, in main, not the
 * unportable wmain()), we have to do the conversion ourself. For the
 * record, most (if not all) other OS support UTF-8 in their syscall (and it is
 * no wonder, given the advantages it brings over UTF-16).
 *
 * How it works:
 * As you may already know, main() is not real the entry function of an
 * executable. Usually the OS loader setup the dynamic libraries in the
 * userland address space and call the real entry point which will initialize
 * the C runtime (CRT) and do the actual call to main after that. The CRT
 * initialization bit usually appears as static library or directly as object
 * files (or both) linked with the actual executable. Those init bits are of
 * course CRT and compiler dependent, however the CRT init bits calls function
 * of the CRT which are a dynamic library with known exported symbol and which
 * are stable over versions. We can use this fact to override certain behavior
 * of the CRT initialization.
 *
 * On Windows, the argument array is initialized just before the C++
 * initializers are called (but after the C initializers) and the way how
 * depends on which version of CRT is used.
 *
 * msvcrt.dll like (mingw and clang and MSVC until VS2013):
 * Those initialize the argument array to pass to main by a call to a function
 * called __getmainargs() exported by the CRT (see crtexe.c from mingw64-crt).
 * This is the actual function that does argument split. This function is
 * invoked from the CRT init bits. Since the symbol is included in the CRT
 * import library which is linked automatically by the compiler after all
 * object files and the direct dependencies of the project being compiled, any
 * symbol of this name in any object file or libraries will take precedence and
 * thus override it.
 *
 * ucrt.dll (MSVC from VS2015):
 * The argument array is initialized in _configure_narrow_argv() whose behavior
 * depends on the function _get_startup_argv_mode(). (See exe_main.cpp,
 * exe_common.inl and vcstartup_internal.h from VC source code of CRT
 * initialization). The _get_startup_argv_mode() function has its default
 * implementation in the CRT init bits but can be overridden by any function of
 * this name that would appear first in the linking order, ie, any function in
 * object file or linked libraries. Please note that new version on mingw64-crt
 * (used by mingw and clang) allows to use ucrt instead of msvcrt, however it
 * is done in a way that the wrapper around urtbase.dll define _getmainargs()
 * so that the CRT init bits remain the same, so the previous case is still
 * applicable for them.
 *
 * NOTE: Do not use it directly in user code. This function is exported for the
 * sole purpose of overriding the function used in mingw64 CRT init code
 * (exported by msvcrt.dll or by their wrapper for ucrtbase.dll).
 *
 * In conclusion, by ensuring that mmlib export __getmainargs() and
 * _get_startup_argv_mode(), which will both initialize the actual UTF-8
 * argument array, we can ensure that any executable linked *directly* with
 * mmlib will have argv in utf-8. If not linked directly, argv with string use
 * the actve codepage will be used.
 */

/**
 * __getmainargs() - Generate the argument array, count and envp for main()
 * @_argc:      pointer to variable receiving the argument count
 * @_argv:      pointer to variable receiving the argument array
 * @_env:       pointer to variable receiving the environment array
 * @_do_wildcard: ignored (it is not the place to do expansion)
 * @_Startinfo: ignored
 *
 * This function perform the setup of the argument array and environment
 * suitable to be passed to main(). Contrary to what is done in msvcrt.dll,
 * the argument are converted from GetCommandLineW() (ie in UTF-16) so that
 * the returned argument strings holds unicode string in UTF-8.
 *
 * Return: 0 in case of success, -1 otherwise
 */
API_EXPORTED
int __getmainargs(int * p_argc, char *** p_argv, char *** p_env,
                  int do_wildcard,  void* startinfo)
{
	(void)do_wildcard;
	(void)startinfo;
	int argc;
	char** argv;

	argc = setup_argv_utf8(&argv);
	if (argc < 0)
		return -1;

	// The first call to getenv will initialize the environ array
	_wgetenv(L"PATH");
	getenv("PATH");

	// Besides output pointers, set __argc and __argv (exported by CRT) in
	// case they would be used in user code (this is not portable but it
	// has existed with CRT on windows for a long time)
	*p_argc = __argc = argc;
	*p_argv = __argv = argv;
	*p_env = environ;
	return 0;
}


// Value equivalent as from vcruntime-startup.h (_crt_argv_mode)
enum {
    ucrt_argv_no_arguments,
    ucrt_argv_unexpanded_arguments,
    ucrt_argv_expanded_arguments,
};


/**
 * _get_startup_argv_mode() - get argv population mode, used during executable startup
 *
 * This function is meant to override the default implementation in CRT init
 * bits. It ensures that command line is split in array of UTF-8 argument
 * strings and if successful, __argc and __argv (from CRT dll) are initialized
 * and prevent by its return value ucrt to overwrite __argc and __argv.
 *
 * NOTE: Do not use it directly in user code. This function is exported for the
 * sole purpose of overriding the function used in MSVC CRT init code (exported
 * by ucrt.lib or by any setargv*.obj).
 *
 * Return: ucrt_argv_no_arguments (0) if no argument parsing must be done,
 * ucrt_argv_unexpanded_arguments(1) if argument parsing must be done but
 * without wildcard expansion.
 */
API_EXPORTED
int _get_startup_argv_mode(void)
{
	int argc;
	char** argv;

	argc = setup_argv_utf8(&argv);

	// If something fails, lets give normal processing in ucrt a try
	if (argc < 0)
		return ucrt_argv_unexpanded_arguments;

	// It is necessary to set __argc and __argv (exported from CRT) here:
	// this is what CRT init use when calling main() if compiled by MSVC
	__argc = argc;
	__argv = argv;

	// The argument array setup is already done, so we pretend that the CRT
	// do not has to do it
	return ucrt_argv_no_arguments;
}
