/*
	@mindmaze_header@
*/
#ifndef ERROR_INTERNAL_H
#define ERROR_INTERNAL_H

#define MM_ERROR_IGNORE 0x01
#define MM_ERROR_NOLOG  0x02
#define MM_ERROR_ALL    0xffffffff

#define MM_ERROR_SET    0xffffffff
#define MM_ERROR_UNSET  0x00000000

int mm_error_set_flags(int flags, int mask);

#endif /* ERROR_INTERNAL_H */
