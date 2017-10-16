/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include "utils-win32.h"

#include <fcntl.h>

#include "mmerrno.h"
#include "mmlog.h"

static
int set_access_mode(struct w32_create_file_options* opts, int oflags)
{
	switch(oflags & (_O_RDONLY|_O_WRONLY| _O_RDWR)) {
	case _O_RDONLY:
		opts->access_mode = GENERIC_READ;
		break;

	case _O_WRONLY:
		opts->access_mode = GENERIC_WRITE;
		break;

	case _O_RDWR:
		opts->access_mode = (GENERIC_READ | GENERIC_WRITE);
		break;

	default:
		mm_raise_error(EINVAL, "Invalid combination of file access mode");
		return -1;
	}

	return 0;
}


static
int set_creation_mode(struct w32_create_file_options* opts, int oflags)
{
	switch (oflags & (_O_TRUNC|_O_CREAT|_O_EXCL)) {
	case 0:
	case _O_EXCL:
		opts->creation_mode = OPEN_EXISTING;
		break;

	case _O_CREAT:
		opts->creation_mode = OPEN_ALWAYS;
		break;

	case _O_TRUNC:
	case _O_TRUNC|_O_EXCL:
		opts->creation_mode = TRUNCATE_EXISTING;
		break;

	case _O_CREAT|_O_EXCL:
	case _O_CREAT|_O_TRUNC|_O_EXCL:
		opts->creation_mode = CREATE_NEW;
		break;

	case _O_CREAT|_O_TRUNC:
		opts->creation_mode = CREATE_ALWAYS;
		break;

	default:
		mm_crash("Previous cases should have covered all possibilities");
	}

	return 0;
}


LOCAL_SYMBOL
int set_w32_create_file_options(struct w32_create_file_options* opts, int oflags)
{
	if ( set_access_mode(opts, oflags)
	  || set_creation_mode(opts, oflags) )
		return -1;

	opts->share_flags = FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE;
	opts->file_attribute = FILE_ATTRIBUTE_NORMAL;
	return 0;
}


