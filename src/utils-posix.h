/*
 * @mindmaze_header@
 */
#ifndef UTILS_POSIX_H
#define UTILS_POSIX_H

static inline
mode_t filter_default(int mode)
{
	if (mode & MODE_DEF)
		return (mode & MODE_EXEC) ? 0777 : 0666;

	return mode;
}

#endif /* ifndef UTILS_POSIX_H */
