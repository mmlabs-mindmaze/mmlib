/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _XOPEN_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <getopt.h>
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>

#include "license.h"


char homecfg[128];

static const char* hwfile;
static char privkey[256];
static char sigdir[256];
static char certfile[256];


static
void init_globals(void)
{
	size_t len = sizeof(homecfg);
	const char* envvar = getenv("XDG_CONFIG_HOME");

	if (envvar)
		strncpy(homecfg, envvar, len);
	else
		snprintf(homecfg, len, "%s/.config", getenv("HOME"));
	
	sprintf(privkey, "%s/mindmaze/default.key", homecfg);
	sprintf(certfile, "%s/mindmaze/default.crt", homecfg);
	sprintf(sigdir, "%s/mindmaze", homecfg);
}


enum {
	CERT,
	PRIVKEY,
	SIGDIR,
	HWFILE,
};

static const struct option opt_str[] = {
	{"cert", required_argument, NULL, CERT},
	{"privkey", required_argument, NULL, PRIVKEY},
	{"signature-dir", required_argument, NULL, SIGDIR},
	{"hw-file", required_argument, NULL, HWFILE},
	{"help", 0, NULL, 'h'},
	{NULL}
};

static
void print_usage(const char* execname, int success)
{
	fprintf(stderr,	"Usage: \n"
	        "   %s [options] <allow|check> \n"
	        "options:\n"
	        "\t--cert=FILE         : path of the X509 certificate"
		" (default: %s/mindmaze/default.crt)\n"
	        "\t--privkey=FILE      : path of the private key"
		" (default: %s/mindmaze/default.key)\n"
		"\t--signature-dir=DIR : folder where the signature will "
	        "be put (default: %s/mindmaze)\n"
		"\t--hw-filer=FILE     : the hw dump used for signature "
		"(default: output of 'lspci -n')\n"
		"\t--help | -h         : display this help\n",
		execname, homecfg, homecfg, homecfg);
	exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}


static
const char* parse_options(int argc, char *argv[])
{
	int c;
	int option_index = 0;

	while (1) {
		c = getopt_long(argc, argv, "h", opt_str, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case CERT:	strcpy(certfile, optarg); break;
		case PRIVKEY:	strcpy(privkey, optarg); break;
		case HWFILE:	hwfile = optarg; break;
		case SIGDIR:	strcpy(sigdir, optarg); break;
		case 'h':
		case '?':
			print_usage(argv[0], (c == 'h') ? 1 : 0);
		}
	}

	if (optind+1 != argc) {
		fprintf(stderr, "Too %s arguments.\n",
		                    (optind+1 > argc) ? "few" : "many");
		print_usage(argv[0], 0);
	}

	return argv[optind];
}


int main(int argc, char* argv[])
{
	int r = 0;
	const char* cmd;

	init_globals();
	cmd = parse_options(argc, argv);

	gnutls_global_init();
	if (!strcmp("allow", cmd)) {
		gen_signature(hwfile, sigdir, certfile, privkey);
	} else if (!strcmp("check", cmd)) {
		r = check_signature(hwfile);
		fprintf(stderr, "signature verification %s\n",
		                r ? "failed" : "succeeded");
	} else {
		fprintf(stderr, "Unknown command: %s\n", cmd);
		r = -1;
	}
	gnutls_global_deinit();

	return (r == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

