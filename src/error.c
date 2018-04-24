/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmerrno.h"
#include "mmlog.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

#include "nls-internals.h"
#include "error-internal.h"

#ifndef thread_local
#  if defined(__GNUC__)
#    define thread_local __thread
#  elif defined(_MSC_VER)
#    define thread_local __declspec(thread)
#  else
#    error Do not know how to specify thread local attribute
#  endif
#endif

struct errmsg_entry {
	int errnum;
	const char* msg;
};

/**************************************************************************
 *                                                                        *
 *                       Messages definition                              *
 *                                                                        *
 **************************************************************************/
static const struct errmsg_entry error_tab[] = {
	{.errnum = MM_EDISCONNECTED,
	 .msg = N_("The acquisition module has been disconnected.")},
	{.errnum = MM_EUNKNOWNUSER, .msg = N_("User unknown")},
	{.errnum = MM_EWRONGPWD, .msg = N_("Wrong password")},
	{.errnum = MM_EWRONGSTATE, .msg = N_("Object in wrong state")},
	{.errnum = MM_ETOOMANY,
	 .msg = N_("Too many entities have been requested")},
	{.errnum = MM_ENOTFOUND, .msg = N_("Object not found")},
	{.errnum = MM_EBADFMT, .msg = N_("Bad format")},
	{.errnum = MM_ENOCALIB, .msg = N_("Calibration needed")},
	{.errnum = MM_ENOINERTIAL, .msg = N_("Hand trackers not detected.\n"
		"Please ensure the USB dongle is connected \nand the sensors are switched on")},
	{.errnum = MM_ECAMERROR, .msg = N_("Communication error with camera hardware.")},
};

#define NUM_ERROR_ENTRY	(sizeof(error_tab)/sizeof(error_tab[0]))

/**************************************************************************
 *                                                                        *
 *                           Implementation                               *
 *                                                                        *
 **************************************************************************/
MM_CONSTRUCTOR(translation)
{
	_domaindir(LOCALEDIR);
}


static
const char* get_mm_errmsg(int errnum)
{
	int i = errnum - error_tab[0].errnum;
	return _(error_tab[i].msg);
}


API_EXPORTED
const char* mmstrerror(int errnum)
{
	if ((errnum < error_tab[0].errnum)
	  || (errnum > error_tab[NUM_ERROR_ENTRY-1].errnum))
		return strerror(errnum);

	return get_mm_errmsg(errnum);
}


#ifdef _WIN32
static
int strerror_r(int errnum, char *strerrbuf, size_t buflen)
{
	return strerror_s(strerrbuf, buflen, errnum);
}
#endif


/**
 * mmstrerror_r() - Get description for error code (reentrant)
 * @errnum:     error to describe
 * @buf:        buffer to which the description should be written
 * @buflen:     buffer size of @buf
 *
 * Return: 0 is in case of success, -1 otherwise.
 */
API_EXPORTED
int mmstrerror_r(int errnum, char *buf, size_t buflen)
{
	const char* msg;
	size_t msglen, trunclen;

	if ((errnum < error_tab[0].errnum)
	  || (errnum > error_tab[NUM_ERROR_ENTRY-1].errnum))
		return strerror_r(errnum, buf, buflen);

	if (buflen < 1) {
		errno = ERANGE;
		return -1;
	}

	msg = get_mm_errmsg(errnum);

	msglen = strlen(msg)+1;
	trunclen = (buflen < msglen) ? buflen : msglen;
	memcpy(buf, msg, trunclen-1);
	buf[trunclen-1] = '\0';

	if (trunclen != msglen) {
		errno = ERANGE;
		return -1;
	}

	return 0;
}


/******************************************************************
 *                                                                *
 *                    Error state API                             *
 *                                                                *
 ******************************************************************/

struct error_info {
	int flags;              //< flags to finetune error handling
	int errnum;             //< error class (standard and mindmaze errno value)
	char extended_id[64];   //< message to display to end user if has not been caught before
	char module[32];        //< module that has generated the error
	char location[256];     //< which function/file/line has generated the error
	char desc[256];         //< message intended to developer
};


// info of the last error IN THE THREAD
static thread_local struct error_info last_error;

/**
 * mm_error_set_flags() - set the error flags
 * @flags:                the flags to add
 * @mask:                 mask applied on the flags
 *
 * Possible flags values are MM_ERROR_IGNORE and MM_ERROR_NOLOG.
 * Additionally, mask can be set to MM_ERROR_ALL.
 *
 * For example use previous = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG)
 * to stop logging errors. The previous variable will contain the original
 * flag variable state, which can then be restored using
 * mm_error_set_flags(previous, MM_ERROR_NOLOG).
 *
 * Return: the previous error_info flags
 */
LOCAL_SYMBOL
int mm_error_set_flags(int flags, int mask)
{
	struct error_info* state = &last_error;
	int previous;

	previous = state->flags;
	state->flags = (mask & flags) | (~mask & previous);

	return previous;
}


/**
 * mm_raise_error_vfull() - set and log an error using a va_list
 * @errnum:     error class number
 * @module:     module name
 * @func:       function name at the origin of the error
 * @srcfile:    filename of source code at the origin of the error
 * @srcline:    line number of file at the origin of the error
 * @extid:      extended error id (identifier of a specific error case)
 * @desc_fmt:   description intended for developper (vprintf-like extensible)
 * @args:       va_list of arguments for @desc
 *
 * Exactly the same as mm_raise_error_full() but using a va_list to pass
 * argument to the format passed in @desc.
 *
 * Return: 0 if @errnum is 0, -1 otherwise.
 */
API_EXPORTED
int mm_raise_error_vfull(int errnum, const char* module, const char* func,
                        const char* srcfile, int srcline,
                        const char* extid,
                        const char* desc_fmt, va_list args)
{
	struct error_info* state;
	int flags;

	if (!errnum)
		return 0;

	if (!module)
		module = "unknown";

	if (!func)
		func = "unknown";

	if (!srcfile)
		srcfile = "unknown";

	if (!extid)
		extid = "";

	state = &last_error;

	// Check that error should not be ignored
	if (state->flags & MM_ERROR_IGNORE)
		return -1;

	// Copy the fields that don't need formatting
	state->errnum = errnum;
	strncpy(state->module, module, sizeof(state->module)-1);
	strncpy(state->extended_id, extid, sizeof(state->extended_id)-1);

	// format source location field
	snprintf(state->location, sizeof(state->location), "%s() in %s:%i", func, srcfile, srcline);

	// format description
	vsnprintf(state->desc, sizeof(state->desc), desc_fmt, args);

	// Set errno for backward compatibility, ie case of module that has
	// been updated to use mm_error* but whose client code (user of this
	// module) is not using yet mm_error*
	errno = errnum;

	if (state->flags & MM_ERROR_NOLOG)
		return -1;

	// Log error but ignore any error that could occur while logging:
	// either ways there would be nothing that can be done about it, but
	// more importantly we do not want to overwrite the error being set
	// by the user.
	flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);
	mmlog_log(MMLOG_ERROR, module, "%s (%s)", state->desc, state->location);
	mm_error_set_flags(flags, MM_ERROR_IGNORE);

	return -1;
}


/**
 * mm_raise_error_full() - set and log an error (function backend)
 * @errnum:     error class number
 * @module:     module name
 * @func:       function name at the origin of the error
 * @srcfile:    filename of source code at the origin of the error
 * @srcline:    line number of file at the origin of the error
 * @extid:      extended error id (identifier of a specific error case)
 * @desc_fmt:   description intended for developper (printf-like extensible)
 *
 * This function is the actual function invoked by the mm_raise_error() and
 * mm_raise_error_with_extid() macros. You are advised to use the macros instead
 * unless you want to build your own wrapper.
 *
 * Return: 0 is @errnum is 0, -1 otherwise.
 */
API_EXPORTED
int mm_raise_error_full(int errnum, const char* module, const char* func,
                      const char* srcfile, int srcline,
                      const char* extid, const char* desc_fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, desc_fmt);
	ret = mm_raise_error_vfull(errnum, module, func, srcfile, srcline,
	                           extid, desc_fmt, args);
	va_end(args);

	return ret;
}


/**
 * mm_save_errorstate() - Save the error state on an opaque data holder
 * @state:      data holder of the error state
 *
 * Use this function to save the current error state to data holder pointed by
 * @state. The content of @state may be copied around even between threads and
 * different processes.
 *
 * Return: 0 (cannot fail)
 *
 * The reciprocal of this function is mm_set_errorstate().
 */
API_EXPORTED
int mm_save_errorstate(struct mm_error_state* state)
{
	assert(sizeof(*state) >= sizeof(last_error));

	memcpy(state, &last_error, sizeof(last_error));
	return 0;
}


/**
 * mm_set_errorstate() - Save the error state of the calling thread
 * @state:      pointer to the data holding of the error state
 *
 * Use this function to restore the error state of the calling thread from the
 * information pointed by @state. Combined with mm_save_errorstate(), you:
 * - handle an error from a called function and recover the error state before
 * the failed function
 * - Copy the error state of a failed function whose call may have been
 * offloaded to a different thread or even different process
 *
 * Return: 0 (cannot fail)
 *
 * The reciprocal of this function is mm_save_errorstate().
 */
API_EXPORTED
int mm_set_errorstate(const struct mm_error_state* state)
{
	assert(sizeof(*state) >= sizeof(last_error));

	memcpy(&last_error, state, sizeof(last_error));

	// Set errno for backward compatibility, ie case of module that has
	// been updated to use mm_error* but whose client code (user of this
	// module) is not using yet mm_error*
	errno = last_error.errnum;

	return 0;
}


/**
 * mm_print_lasterror() - display last error info on standard output
 * @info:       string describing the context where the error has been
 *              encountered. It can be enriched by variable argument in the
 *              printf-like style. It may be NULL, in such a case, only the
 *              error state is described.
 */
API_EXPORTED
void mm_print_lasterror(const char* info, ...)
{
	va_list args;

	// Print context info if supplied
	if (info) {
		va_start(args, info);
		vprintf(info, args);
		va_end(args);
		printf("\n");
	}

	// No error state is set, not in errno
	if (!last_error.errnum && !errno) {
		printf("No error found in the state\n");
		return;
	}

	// No error state is set, but something is in errno
	if (!last_error.errnum && errno) {
		printf("Error only found in errno: %i, %s\n",
		       errno, mmstrerror(errno));
		return;
	}

	// Print the error state
	printf("Last error reported:\n"
	       "\terrnum=%i : %s\n"
	       "\tmodule: %s\n"
	       "\tlocation: %s\n"
	       "\tdescription: %s\n"
	       "\textented_id: %s\n",
	       last_error.errnum, mmstrerror(last_error.errnum),
	       last_error.module,
	       last_error.location,
	       last_error.desc,
	       last_error.extended_id);
}


/**
 * mm_get_lasterror_number() - get error number of last error in the thread
 *
 * Return: the error number (0 if no error has been set in the thread)
 */
API_EXPORTED
int mm_get_lasterror_number(void)
{
	return last_error.errnum;
}


/**
 * mm_get_lasterror_desc() - get error description of last error in thread
 *
 * Return: the error description ("" if no error)
 */
API_EXPORTED
const char* mm_get_lasterror_desc(void)
{
	return last_error.desc;
}


/**
 * mm_get_lasterror_location() - get file location of last error in the thread
 *
 * Return: the file location that is at the origin of the error in the format
 * "filename:linenum" ("" if no error)
 */
API_EXPORTED
const char* mm_get_lasterror_location(void)
{
	return last_error.location;
}


/**
 * mm_get_lasterror_extid() - get error extended id of last error in the thread
 *
 * This function provides the extended id of the last error set in the calling
 * thread. The extended id is a string identifier destinated for the UI layer
 * of the software stack to identify a error specific situation(See explanation in mm_raise_error_with_extid()).
 *
 * Please note that not all error report are supposed to report an error
 * extended id (they actually should be a minority). If none has been provided
 * when the last error has been set, the extid provided by this function will
 * be NULL.
 *
 * Return: the error extended id if one has been set by the last error, NULL
 * otherwise.
 */
API_EXPORTED
const char* mm_get_lasterror_extid(void)
{
	// Don't return an empty string if extid is not set
	if (last_error.extended_id[0] == '\0')
		return NULL;

	return last_error.extended_id;
}


/**
 * mm_get_lasterror_module() - module at the source the last error in the thread
 *
 * Return: the module name that is at the origin of the error ("" if no error)
 */
API_EXPORTED
const char* mm_get_lasterror_module()
{
	return last_error.module;
}
