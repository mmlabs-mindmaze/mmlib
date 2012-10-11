/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "mmlog.h"

static pthread_once_t once_init = PTHREAD_ONCE_INIT;
static int maxloglvl = MMLOG_WARN;

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
void init_log(void)
{
	int i;
	const char* envlvl;

	envlvl = getenv("MM_LOG_MAXLEVEL");
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
	char timestamp[32];
	va_list args;
	
	// Configure max level from environment
	pthread_once(&once_init, init_log);

	// Do not log something higher than the max level set by environment
	if (lvl > maxloglvl || lvl < 0)
		return;
	
	// format time stamp
	ts = time(NULL);
	localtime_r(&ts, &tm);
	strftime(timestamp, sizeof(timestamp), "%d/%m/%y %T", &tm);
	
	// printf message
	fprintf(stderr, "%s %s %s: ", timestamp, location, loglevel[lvl]);
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);
	fputc('\n', stderr);
}

