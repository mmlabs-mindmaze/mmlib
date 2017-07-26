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
#include <stdarg.h>
#include <assert.h>

#include "nls-internals.h"

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
	int errnum;             //< error class (standard and mindmaze errno value)
	char extended_id[64];   //< message to display to end user if has not been caught before
	char module[32];        //< module that has generated the error
	char location[256];     //< which function/file/line has generated the error
	char desc[256];         //< message intended to developer
};


// info of the last error IN THE THREAD
static thread_local struct error_info last_error;

API_EXPORTED
int mm_raise_error_full(int errnum, const char* module, const char* func,
                      const char* srcfile, int srcline,
                      const char* extid, const char* desc_fmt, ...)
{
	va_list args;
	struct error_info* state;

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

	// Copy the fields that don't need formatting
	state->errnum = errnum;
	strncpy(state->module, module, sizeof(state->module)-1);
	strncpy(state->extended_id, extid, sizeof(state->extended_id)-1);

	// format source location field
	snprintf(state->location, sizeof(state->location), "%s() in %s:%i", func, srcfile, srcline);

	// format description
	va_start(args, desc_fmt);
	vsnprintf(state->desc, sizeof(state->desc), desc_fmt, args);
	va_end(args);

	// Set errno for backward compatibility, ie case of module that has
	// been updated to use mm_error* but whose client code (user of this
	// module) is not using yet mm_error*
	errno = errnum;

	mmlog_log(MMLOG_ERROR, module, "%s (%s)", state->desc, state->location);
	return -1;
}

API_EXPORTED
int mm_save_errorstate(struct mm_error_state* state)
{
	assert(sizeof(*state) >= sizeof(last_error));

	memcpy(state, &last_error, sizeof(last_error));
	return 0;
}

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

API_EXPORTED
int mm_get_lasterror_number(void)
{
	return last_error.errnum;
}


API_EXPORTED
const char* mm_get_lasterror_desc(void)
{
	return last_error.desc;
}

API_EXPORTED
const char* mm_get_lasterror_location(void)
{
	return last_error.location;
}


API_EXPORTED
const char* mm_get_lasterror_extid(void)
{
	// Don't return an empty string if extid is not set
	if (last_error.extended_id[0] == '\0')
		return NULL;

	return last_error.extended_id;
}


API_EXPORTED
const char* mm_get_lasterror_module()
{
	return last_error.module;
}
