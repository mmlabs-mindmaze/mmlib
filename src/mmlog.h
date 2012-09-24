/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#ifndef MMLOG_H
#define MMLOG_H

#define MMLOG_NONE	-1
#define MMLOG_FATAL	0
#define MMLOG_ERROR	1
#define MMLOG_WARN	2
#define MMLOG_INFO	3
#define MMLOG_DEBUG	4

#ifndef MMLOG_MAXLEVEL
#  define MMLOG_MAXLEVEL	MMLOG_WARN
#endif


#if defined __cplusplus
# define MMLOG_VOID_CAST static_cast<void>
#else
# define MMLOG_VOID_CAST (void)
#endif


#if MMLOG_MAXLEVEL >= MMLOG_FATAL
#  define mmlog_fatal(module, msg, ...)	mmlog_log(MMLOG_FATAL, __FILE__, __LINE__, module, msg,  ## __VA_ARGS__)
#else
#  define mmlog_fatal(module, msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_ERROR
#  define mmlog_error(module, msg, ...)	mmlog_log(MMLOG_ERROR, __FILE__, __LINE__, module, msg,  ## __VA_ARGS__)
#else
#  define mmlog_error(module, msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_WARN
#  define mmlog_warn(module, msg, ...)	mmlog_log(MMLOG_WARN, __FILE__, __LINE__, module, msg,  ## __VA_ARGS__)
#else
#  define mmlog_warn(module, msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_INFO
#  define mmlog_info(module, msg, ...)	mmlog_log(MMLOG_INFO, __FILE__, __LINE__, module, msg,  ## __VA_ARGS__)
#else
#  define mmlog_info(module, msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_DEBUG
#  define mmlog_trace(module, msg, ...)	mmlog_log(MMLOG_DEBUG, __FILE__, __LINE__, module, msg,  ## __VA_ARGS__)
#else
#  define mmlog_trace(module, msg, ...) MMLOG_VOID_CAST(0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void mmlog_log(int lvl, const char* file, int line,
               const char* module, const char* msg, ...);


#ifdef __cplusplus
}
#endif

#endif /*MMLOG_H*/
