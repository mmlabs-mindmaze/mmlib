/*
 * @mindmaze_header@
 */

#include <mmargparse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mmsysio.h>

struct config {
	const char* detach_flag;
	unsigned int num_instance;
	const char* ip;
	const char* use_local_storage;
};
static
struct config cfg = {
	.num_instance = 10,
	.ip = "127.0.0.1",
};

#define LOREM_IPSUM "Lorem ipsum dolor sit amet, consectetur adipiscing"   \
	"elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua." \
	"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi " \
	"ut aliquip ex ea commodo consequat..."

#define DEFAULT_PATH "/default/path"

static
struct mmarg_opt cmdline_optv[] = {
	{"detach", MMOPT_NOVAL, "set", {.sptr = &cfg.detach_flag},
	 "detach server process."},
	{"n|num-instance", MMOPT_NEEDUINT, NULL, {.uiptr = &cfg.num_instance},
	 "Server can accommodate up to @NUM client simultaneously. Here is "
	 "more explanation to test text wrapping. " LOREM_IPSUM},
	{"l|use-local-storage", MMOPT_OPTSTR, DEFAULT_PATH, {NULL},
	 "Use local storage located at @PATH which must exist. "
	 "If unspecified @PATH is assumed "DEFAULT_PATH "."},
	{.name = "i", .flags = MMOPT_NEEDSTR, .defval = NULL, {.sptr = &cfg.ip},
	 .desc = "IP address of remote server. @ADDR must have dotted form."},
};


/**
 * parse_option_cb() - validate some option value and parse other
 * @opt:        parser configuration of option recognized
 * @value:      value about to be set for option
 * @data:       callback data
 * @state:      flags indicating the state of option parsing.
 *
 * Return: 0 is parsing must continue, -1 if error has been detect and
 * parsing must stop.
 */
static
int parse_option_cb(const struct mmarg_opt* opt, union mmarg_val value,
                    void* data, int state)
{
	struct config* conf = data;
	(void)state;

	switch (mmarg_opt_get_key(opt)) {
	case 'n':
		if (value.ui < 1) {
			fprintf(stderr,
			        "Server must support at least 1 instance\n");
			return -1;
		}

		// We don't set value here, since variable to set is already
		// configured in option setup ({.strptr = &cfg.ip})
		return 0;

	case 'l':
		if (mm_check_access(value.str, F_OK) != 0) {
			fprintf(stderr,
			        "storage file %s does not exist\n",
			        value.str);
			return -1;
		}

		conf->use_local_storage = value.str;
		return 0;

	default:
		return 0;
	}
}


int main(int argc, char* argv[])
{
	int i, arg_index;
	struct mmarg_parser parser = {
		.doc = LOREM_IPSUM,
		.args_doc = "[options] cmd argument\n[options] hello",
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.cb = parse_option_cb,
		.cb_data = &cfg,
		.execname = argv[0],
	};


	arg_index = mmarg_parse(&parser, argc, argv);

	fprintf(stdout, "options used:\n\tdetach_flag: %s\n\tinstance: %u\n"
	        "\tserver address: %s\n\tuse local path: %s\n",
	        cfg.detach_flag, cfg.num_instance,
	        cfg.ip, cfg.use_local_storage);

	fprintf(stdout, "Execute ");
	for (i = arg_index; i < argc; i++)
		fprintf(stdout, "%s ", argv[i]);

	fputc('\n', stdout);

	return EXIT_SUCCESS;
}
