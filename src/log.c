/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "mmlog.h"

#ifndef MMLOG_LINE_MAXLEN
#define MMLOG_LINE_MAXLEN	256
#endif

static int maxloglvl = MMLOG_INFO;

static
const char* const loglevel[] = {
	[MMLOG_FATAL] = "FATAL",
	[MMLOG_ERROR] = "ERROR",
	[MMLOG_WARN] = "WARN",
	[MMLOG_INFO] = "INFO",
	[MMLOG_DEBUG] = "DEBUG",
};
#define NLEVEL (sizeof(loglevel)/sizeof(loglevel[0]))

static
void init_log(void) __attribute__ ((constructor));

static
void init_log(void)
{
	int i;
	const char* envlvl;

	envlvl = getenv("MMLOG_MAXLEVEL");
	if (!envlvl)
		return;
	
	for (i=0; i<(int)NLEVEL; i++) {
		if (!strcmp(loglevel[i], envlvl)) {
			maxloglvl = i;
			return;
		}
	}
	
	// If we reach each, unknown level has been set through environment
	// In that case, it should be equivalent to no log
	maxloglvl = MMLOG_NONE;		
}


API_EXPORTED
void mmlog_log(int lvl, const char* location, const char* msg, ...)
{
	time_t ts;
	struct tm tm;
	char* cbuf;
	size_t len, rlen;
	ssize_t r;
	va_list args;
	char buff[MMLOG_LINE_MAXLEN];
	
	// Do not log something higher than the max level set by environment
	if (lvl > maxloglvl || lvl < 0)
		return;
	
	rlen = sizeof(buff);

	// format time stamp
	ts = time(NULL);
	localtime_r(&ts, &tm);
	len = strftime(buff, sizeof(buff)-1, "%d/%m/%y %T", &tm);
	cbuf = buff + len;
	rlen -= len;
	
	// format message header message
	len = snprintf(cbuf, rlen-1, " %-5s %-16s : ", loglevel[lvl], location);
	cbuf += len;
	rlen -= len;
	
	// Format provided info
	va_start(args, msg);
	len = vsnprintf(cbuf, rlen-1, msg, args);
	va_end(args);
	cbuf[len++] = '\n';
	rlen -= len;

	// Write message on log file descriptor
	cbuf = buff;
	len = sizeof(buff)-rlen;
	do {
		if ((r = write(STDERR_FILENO, cbuf, len)) < 0)
			return;
		len -= r;
		cbuf += r;
	} while (len);
}

