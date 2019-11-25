/*
 *      @mindmaze_header@
 */
#ifndef VOLUME_WIN32_H
#define VOLUME_WIN32_H

#include <uchar.h>

#include "mmsysio.h"

enum {
	FSTYPE_UNKNOWN,
	FSTYPE_NTFS,
	FSTYPE_FAT32,
	FSTYPE_EXFAT,
};


struct volume {
	mm_dev_t dev;
	char16_t* guid_path;
	int fs_type;
};


const struct volume* get_volume_from_dev(mm_dev_t dev);
int volume_get_trash_prefix_u16(const struct volume* vol, char16_t* path);


#endif
