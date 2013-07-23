/*
    Copyright (C) 2012-2013  MindMaze SA
    All right reserved

    Author: Guillaume Monnard <guillaume.monnard@mindmaze.ch>
*/

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmtype.h"

API_EXPORTED
size_t mmimage_buffer_size(const mmimage* img)
{
	return ( img->width * img->height * img->nch
		* ( img->depth & MM_SIGN_MASK ) + 7 ) / 8;
}

