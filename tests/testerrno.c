/*
   @mindmaze_header@
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
	print_errno_info(MM_EUNKNOWNUSER);
	print_errno_info(MM_EWRONGPWD);
	print_errno_info(MM_ECAMERROR);

	return EXIT_SUCCESS;
}
