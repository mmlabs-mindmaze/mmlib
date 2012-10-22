/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#ifndef NLS_INTERNALS_H
#define NLS_INTERNALS_H

#include <libintl.h>

#define N_(msg)	msg
#define _(msg)	gettext(PACKAGE_NAME, msg)

#endif /* NLS_INTERNALS_H */
