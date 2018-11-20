/*
   @mindmaze_header@
*/
#ifndef MMERRNO_H
#define MMERRNO_H

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include "mmtype.h"
#include "mmpredefs.h"

/**
 * DOC: error codes
 *
 * The error code should be seen as an error class that describes roughly
 * the type of error encountered. Depending on their context, some layer can
 * handle a specific error reported by their callee (maybe raised in much
 * lower layer). It will thus use the code to filter the error it handle and
 * propagate to the upper layers the others.
 *
 * Mindmaze software defines a number of error in addition to the usual one
 * defined in errno.h by the system to address the case not covered by the
 * system error. In the following table, you will find the list of typical
 * error that you will most likely use (or receive)
 *
 * EINVAL:
 *   Invalid argument. This is used to indicate various kinds of problems
 *   with passing the wrong argument to a library function.
 *
 * ENOSYS:
 *   Function not implemented. This indicates that the function called is not
 *   implemented at all, either in the C library, in the operating
 *   system or in a Mindmaze lib (typically you try to access a set of
 *   functionality that are disabled by a compilation flag). When you get
 *   this error, you can be sure that this particular function will always
 *   fail with ENOSYS unless you install a new version of the operating
 *   system.
 *
 * ENOTSUP:
 *   Not supported. A function returns this error when certain parameter
 *   values are valid, but the functionality they request is not available.
 *   This can mean that the function does not implement a particular command
 *   or option value or flag bit at all. For functions that operate on some
 *   object given in a parameter, such as a file descriptor or a port, it
 *   might instead mean that only that specific object (file descriptor,
 *   port, etc.) is unable to support the other parameters given; different
 *   file descriptors might support different ranges of parameter values.  If
 *   the entire function is not available at all in the implementation, it
 *   returns ENOSYS instead.
 *
 * ETIMEDOUT:
 *   An operation with a specified timeout received no response during the
 *   timeout period.
 *
 * EPIPE:
 *   Broken pipe; there is no process reading from the other end of a pipe.
 *   Every library function that returns this error code also generates a
 *   SIGPIPE signal; this signal terminates the program if not handled or
 *   blocked. Thus, your program will never actually see EPIPE unless it has
 *   handled or blocked SIGPIPE.
 *
 * ENOENT:
 *   No such file or directory. This is a “file doesn’t exist” error for
 *   ordinary files that are referenced in contexts where they are expected
 *   to already exist.
 *
 * EACCES:
 *  Permission denied; the file permissions do not allow the attempted
 *  operation.
 *
 * ERANGE:
 *   Insufficient storage was supplied to contain the data.
 *
 * EBUSY:
 *   Resource busy; a resource that can’t be shared is already in use.
 *
 * EIO:
 *   Input/output error; usually used for physical read or write errors.
 *
 * MM_EDISCONNECTED:
 *   Hardware is disconnected or turned off
 *
 * MM_ENOTFOUND:
 *   An entity that are requested cannot be found.
 *
 * MM_EBADFMT:
 *   The format of a file or data is not the one expected.
 *
 * MM_ENOCALIB:
 *   A module need to be calibrated (either it has not been calibrated or
 *   the calibration data is no longer valid).
 *
 * MM_ENOINERTIAL:
 *   Inertial sensors are disconnected
 *
 * MM_ECAMERROR:
 *   Communication error with camera hardware
 */

#define MM_EDISCONNECTED	1000
#define MM_EUNKNOWNUSER		1001
#define MM_EWRONGPWD		1002
#define MM_EWRONGSTATE		1003
#define MM_ETOOMANY		1004
#define MM_ENOTFOUND		1005
#define MM_EBADFMT		1006
#define MM_ENOCALIB		1007
#define MM_ENOINERTIAL		1008
#define MM_ECAMERROR		1009

// Surprisingly some compilers targeting windows fail to define
// ENOTRECOVERABLE error code while they define EOWNERDEAD
#ifdef _WIN32
#  ifndef ENOTRECOVERABLE
#    define ENOTRECOVERABLE 127
#  endif
#  ifndef ENOMSG
#    define ENOMSG 42
#  endif
#endif


#define MM_ERROR_IGNORE 0x01
#define MM_ERROR_NOLOG  0x02
#define MM_ERROR_ALL_ALTERNATE  0xffffffff

#define MM_ERROR_SET    0xffffffff
#define MM_ERROR_UNSET  0x00000000


#ifdef __cplusplus
extern "C" {
#endif

MMLIB_API const char* mmstrerror(int errnum);

MMLIB_API int mmstrerror_r(int errnum, char *buf, size_t buflen);

/**
 * mm_raise_error() - set and log an error
 * @errnum:     error class number
 * @desc:       description intended for developper (printf-like extensible)
 *
 * Set the state of an error in the running thread. If @errnum is 0, the
 * thread error state will kept untouched.
 *
 * Although this can be used like a function, this is a macro which will
 * enrich the error state with the origin of the error (module, function,
 * source code file and line number).
 *
 * This must be called the closest place where the error is detected. Ie, if
 * you call a mindmaze function that sets the error and you, you shall not
 * the error because your callee has failed, the only thing that might be
 * done is this case would be adding a log line (if necessary). On the other
 * side, if you call function from third party, the error state will of course not
 * be set, it will then be your responsibility once you detect the
 * third-party call has failed to set the error state using mm_raise_error().
 *
 * Return: always -1.
 */
#define mm_raise_error(errnum, desc, ...) \
		mm_raise_error_full(errnum, MMLOG_MODULE_NAME, __func__, __FILE__, __LINE__, NULL, desc,  ## __VA_ARGS__ )

#define mm_raise_from_errno(desc, ...) \
	mm_raise_error_full(errno, MMLOG_MODULE_NAME, __func__, __FILE__, __LINE__, NULL, desc ": %s", ##__VA_ARGS__, strerror(errno))

/**
 * mm_raise_error_with_extid() - set and log an error with an extented error id
 * @errnum:     error class number
 * @extid:      extended error id (identifier of a specific error case)
 * @desc:       description intended for developper (printf-like extensible)
 *
 * Same as mm_raise_error() with an extended error id set to @extid. If @extid
 * is NULL, the effect is exactly the same as calling mm_raise_error().
 *
 * An extended error id is a string identifier that is meant to inform the
 * layer that interact with the enduser (like therapist) about the reason of
 * the error. With this identifier, the UI layer can display a error
 * message that makes sense to the end user. Example, the cameralink camera can
 * detect a HW problem due to ESD. When this happens, the acquisition driver
 * will set an hardware error class (like MM_ECAMERROR) with an extid set to
 * "clcam-esd-detected". The UI of the final product will recognise the
 * extended identifier and display to the appropriate message (with the
 * right language) that make sense in the context of the usage of the
 * product and maybe what the enduser has to do.
 *
 * Return: always -1.
 */
#define mm_raise_error_with_extid(errnum, extid, desc, ...) \
		mm_raise_error_full(errnum, MMLOG_MODULE_NAME, __func__, __FILE__, __LINE__, extid, desc,  ## __VA_ARGS__ )


MMLIB_API int mm_raise_error_full(int errnum, const char* module, const char* func,
                                  const char* srcfile, int srcline,
                                  const char* extid, const char* desc, ...);

MMLIB_API int mm_raise_error_vfull(int errnum, const char* module, const char* func,
                                   const char* srcfile, int srcline,
                                   const char* extid, const char* desc, va_list args);

MMLIB_API int mm_error_set_flags(int flags, int mask);
MMLIB_API int mm_save_errorstate(struct mm_error_state* state);
MMLIB_API int mm_set_errorstate(const struct mm_error_state* state);
MMLIB_API void mm_print_lasterror(const char* info, ...);
MMLIB_API int mm_get_lasterror_number(void);
MMLIB_API const char* mm_get_lasterror_desc(void);
MMLIB_API const char* mm_get_lasterror_location(void);
MMLIB_API const char* mm_get_lasterror_extid(void);
MMLIB_API const char* mm_get_lasterror_module(void);

#ifdef __cplusplus
}
#endif

#endif /* MMERRNO */
