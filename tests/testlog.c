/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <mmlog.h>

#define modname "testlog"

int main(void)
{
	int i;

	mmlog_info(modname, "Starting logging...");

	for (i = -5; i<5; i++) {
		if (i != 0)
			mmlog_warn(modname, "Everything is fine (%i)", i);
		else
			mmlog_error(modname, "Null value (i == %i)", i);
	}

	mmlog_info(modname, "Stop logging.");

	return EXIT_SUCCESS;
}
