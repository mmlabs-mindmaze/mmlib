/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmerrno.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "nls-internals.h"

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
};

#define NUM_ERROR_ENTRY	(sizeof(error_tab)/sizeof(error_tab[0]))

/**************************************************************************
 *                                                                        *
 *                           Implementation                               *
 *                                                                        *
 **************************************************************************/
static
void init_translation(void)  __attribute__ ((constructor));

static
void init_translation(void)
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

