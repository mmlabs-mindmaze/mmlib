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

	if (len < 2 || len > MM_SHM_NAME_MAXLEN)
		return false;

	// Only name with leading slash
	if (name[0] != '/')
		return false;

	// prevent "."
	if (len == 2 && name[1] == '.')
		return false;

	// prevent ".."
	if (len == 3 && name[1] == '.' && name[2] == '.')
		return false;

	for (i = 1; i < len; i++) {
		c = name[i];
		if (c == '\\' || c == '/' || c == ':')
			return false;
	}

	return true;
}


#endif /* SHM_INTERNAL_H */
