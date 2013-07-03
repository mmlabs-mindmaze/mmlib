/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#ifndef NLS_INTERNALS_H
#define NLS_INTERNALS_H

#if ENABLE_NLS


#include <libintl.h>

#define N_(msg)	msg
#define _(msg)	dgettext(PACKAGE_NAME, msg)
#define _domaindir(podir)	bindtextdomain(PACKAGE_NAME, podir)


#else // ENABLE_NLS


#define N_(msg) msg
#define _(msg)	msg
#define _domaindir(podir) (void)(0)


#endif // ENABLE_NLS

#endif /* NLS_INTERNALS_H */
