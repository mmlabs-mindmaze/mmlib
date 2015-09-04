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


/**
 * format_log_str() - generate log string on supplied buffer
 * @buffer:     buffer that must receive the log string
 * @blen:       maximum size of @buffer
 * @lvl:        level of the log line
 * @location:   module name at the origin of the log
 * @msg:        format controlling the log message
 * @args:       argument list of supplied for @msg
 *
 * This function generates the log string according to @lvl, @location and
 * the supplied message that must be formatted with respect to the format
 * @msg and the argument list in @args.
 *
 * NOTE: The string is meant to be written as such to log file with write()
 * system call. In consequence, please pay attention that the log string
 * written in @buffer IS NOT null terminated!
 *
 * Return: the number of byte written on @buffer.
 */
static
size_t format_log_str(char* restrict buff, size_t blen,
                      int lvl, const char* restrict location,
                      const char* restrict msg, va_list args)
{
	time_t ts;
	struct tm tm;
	size_t len, rlen;

	rlen = blen;

	// format time stamp
	ts = time(NULL);
	localtime_r(&ts, &tm);
	len = strftime(buff, blen, "%d/%m/%y %T", &tm);
	buff += len;
	rlen -= len;
	
	// format message header message
	len = snprintf(buff, rlen-1, " %-5s %-16s : ", loglevel[lvl], location);
	buff += len;
	rlen -= len;
	
	// Format provided info and append end of line
	len = vsnprintf(buff, rlen, msg, args);
	len = (len < rlen-1) ? len : rlen-1;	// handle truncation case
	buff[len++] = '\n';
	rlen -= len;

	// Return the length of string without null terminator
	return blen - rlen;
}


API_EXPORTED
void mmlog_log(int lvl, const char* location, const char* msg, ...)
{
	ssize_t r;
	size_t len;
	char* cbuf;
	va_list args;
	char buff[MMLOG_LINE_MAXLEN];

	// Do not log something higher than the max level set by environment
	if (lvl > maxloglvl || lvl < 0)
		return;

	// Format log string onto buffer
	va_start(args, msg);
	len = format_log_str(buff, sizeof(buff), lvl, location, msg, args);
	va_end(args);

	// Write message on log file descriptor
	cbuf = buff;
	do {
		if ((r = write(STDERR_FILENO, cbuf, len)) < 0)
			return;
		len -= r;
		cbuf += r;
	} while (len);
}

