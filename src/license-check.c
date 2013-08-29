/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <gnutls/gnutls.h>

#include "license.h"

static
void license_check(void) __attribute__ ((constructor));

static
void license_check(void)
{
	int ret;

	gnutls_global_init();
	ret = check_signature(NULL);
	gnutls_global_deinit();

	if (ret)
		exit(EXIT_FAILURE);
}
