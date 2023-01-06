/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <mmsysio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(_MSC_VER)
#  define raise_sigill  __ud2
#else
#  define raise_sigill  __builtin_trap
#endif


static
void raise_sigfpe(void)
{
#ifdef _WIN32
	RaiseException(EXCEPTION_FLT_DIVIDE_BY_ZERO, 0, 0, NULL);
#else
	raise(SIGFPE);
#endif
}


static
int check_open_files(int argc, char* argv[])
{
	int i;
	int num_fd;
	char line[64];

	for (i = 0; i < argc; i++)
		fprintf(stderr, "%s\n", argv[i]);

	num_fd = atoi(argv[argc-1]);
	fprintf(stderr, "num_fd = %i\n", num_fd);
	for (i = 3; i < 2*num_fd+3; i++) {
		sprintf(line, "fd = %i", i);
		if (mm_write(i, line, strlen(line)) < 0)
			fprintf(stderr, "failed write for fd=%i\n", i);
	}

	return 0;
}


static
int check_signal(int signum)
{
	union {int* iptr; intptr_t v;} val = {.v = 0};

	switch (signum) {
	case SIGABRT: abort();
	case SIGSEGV: printf("%i", *val.iptr); break; // Must raise segfault
	case SIGFPE:  raise_sigfpe(); break;
	case SIGILL:  raise_sigill(); break;
	default:      raise(signum);
	}

	return EXIT_FAILURE;
}


int main(int argc, char* argv[])
{
	if (argc < 2) {
		fprintf(stderr, "child-proc: Missing argument\n");
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "check-open-files") == 0)
		return check_open_files(argc, argv);

	if (strcmp(argv[1], "check-exit") == 0)
		return atoi(argv[2]);

	if (strcmp(argv[1], "check-signal") == 0)
		return check_signal(atoi(argv[2]));

	fprintf(stderr, "child-proc: invalid argument: %s\n", argv[1]);
	return EXIT_FAILURE;
}
