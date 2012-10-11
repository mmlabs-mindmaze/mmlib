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

static
void logged_func(void)
{
	mmlog_warn("logged_func");
}

#undef MMLOG_MODULE_NAME
#define MMLOG_MODULE_NAME "testlog"

int main(void)
{
	int i;

	mmlog_info("Starting logging...");

	for (i = -5; i<5; i++) {
		logged_func();
		if (i != 0)
			mmlog_warn("Everything is fine (%i)", i);
		else
			mmlog_error("Null value (i == %i)", i);
	}

	mmlog_info("Stop logging.");

	return EXIT_SUCCESS;
}
