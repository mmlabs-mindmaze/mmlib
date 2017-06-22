/*
   @mindmaze_header@
*/
#ifndef MMERRNO_H
#define MMERRNO_H

#include <stddef.h>
#include <errno.h>

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


#ifdef __cplusplus
extern "C" {
#endif

const char* mmstrerror(int errnum);
int mmstrerror_r(int errnum, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* MMERRNO */
