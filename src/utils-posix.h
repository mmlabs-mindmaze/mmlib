/*
 * @mindmaze_header@
 */
#ifndef UTILS_POSIX_H
#define UTILS_POSIX_H

/**
 * filter_mode_flags() - construct mode out of mode and flags
 * @mode:        mode argument passed in mmlib API
 *
 * This function interprets the flags added introduced by mmlib API (MODE_DEF,
 * MODE_XDEF, ...) in @mode and adjusts the permission value to match the
 * intended behavior indicated by flags. If no mmlib specific flags have been
 * passed to @mode, it is returned untouched.
 *
 * Returns: a mode value that can be supplied to open(), mkdir() or shm_open()
 */
static inline
mode_t filter_mode_flags(int mode)
{
	if (mode & MODE_DEF)
		return (mode & MODE_EXEC) ? 0777 : 0666;

	return mode;
}

#endif /* ifndef UTILS_POSIX_H */
