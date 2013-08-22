/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmtype.h"

API_EXPORTED
size_t mmimage_buffer_size(const mmimage* img)
{
	return ( img->width * img->height * img->nch
		* ( img->depth & ~MM_DEPTH_SIGN ) + 7 ) / 8;
}

