/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "mmsysio.h"
#include "mmlog.h"

// Define STDERR_FILENO if not (may happen with some compiler for Windows)
#ifndef STDERR_FILENO
#  define STDERR_FILENO 2
#endif

#ifdef _MSC_VER
#  define restrict __restrict
#  define ssize_t int
#endif

#if _WIN32
#  if HAS_LOCALTIME_S
#    define localtime_r(time, tm) localtime_s((tm), (time))
#  else
#    define localtime_r(time, tm) do {*(tm) = *(localtime(time));} while (0)
#  endif
#endif

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

MM_CONSTRUCTOR(init_log)
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
 * @buff:       buffer that must receive the log string
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
	len = strftime(buff, blen, "%d/%m/%y %H:%M:%S", &tm);
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


/**
 * mmlog_log() - Add a formatted message to the log file
 * @lvl:        log level.
 * @location:   origin of the log message.
 * @msg:        log message.
 *
 * Writes an entry in the log following the suggested format by the Mindmaze
 * standard: date, time, origin, level, message. The origin part is specified
 * by the @location parameter. The severity part is defined by the @lvl
 * parameter which must one of this value listed from the most critical to the
 * least one:
 *   MMLOG_FATAL
 *   MMLOG_ERROR
 *   MMLOG_WARN
 *   MMLOG_INFO
 *   MMLOG_DEBUG
 *
 * The message part of log entry is formed according the format specified by the
 * @msg parameters which convert the formatting and conversion of the optional
 * argument list of the function. As the format specified by @msg follows the
 * one of the @sprintf function.
 *
 * If the parameter lvl is less critical than the environment variable
 * @MMLOG_MAXLEVEL, the log entry will not be written to log and simply ignored.
 *
 * Usually, users do not call mmlog_log() directly but use one of the following
 * macros: mmlog_fatal(), mmlog_error(), mmlog_warn(), mmlog_info(),
 * mmlog_debug()
 *
 * Return: None
 *
 * ENVIRONMENT: You can control the output on the log at runtime using the
 * environment variable @MMLOG_MAXLEVEL. It specifies the maximum level of
 * severity that must be written on the log. It must be set to one of these
 * values:
 *   NONE
 *   FATAL
 *   ERROR
 *   WARN
 *   INFO
 *   DEBUG
 *
 * A value different from the one listed above, the maximum level output on the
 * log is WARN.
 *
 * mmlog_log() is thread-safe.
 *
 * See: sprintf(), mmlog_fatal(), mmlog_error(), mmlog_warn(),
 * mmlog_info(), mmlog_debug()
 */
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
		if ((r = mm_write(STDERR_FILENO, cbuf, len)) < 0)
			return;
		len -= r;
		cbuf += r;
	} while (len);
}


/**
 * mmlog_set_maxlvl() - set maximum log level
 * @lvl: log level to set
 *
 * Return: previous log level
 */
API_EXPORTED
int mmlog_set_maxlvl(int lvl)
{
	int rv = maxloglvl;
	maxloglvl = lvl;
	return rv;
}
