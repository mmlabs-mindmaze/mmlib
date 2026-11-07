/*
 * @mindmaze_header@
 */
#ifndef SHM_INTERNAL_H
#define SHM_INTERNAL_H

#include <stdbool.h>
#include <string.h>
#include "mmsysio.h"


static
bool is_shm_name_valid(const char* name)
{
	int i, len = strlen(name);
	char c;

	if (len > MM_SHM_NAME_MAXLEN)
		return false;

	for (i = 0; i < len; i++) {
		c = name[i];
		if (c == '\\' || c == '/' || c == '.')
			return false;
	}

	return true;
}


#define SHM_INTERNAL_H
