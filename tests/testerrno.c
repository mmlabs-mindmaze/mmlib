/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <mmerrno.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#define print_errno_info(errnum)	\
	printf("%s (%i) : %s\n", #errnum , errnum, mmstrerror(errnum))

int main(void)
{
	setlocale(LC_ALL, "");

	print_errno_info(MM_EDISCONNECTED);

	return EXIT_SUCCESS;
}
