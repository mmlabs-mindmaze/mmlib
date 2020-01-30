/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmprofile.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define OUTFD	1 //STDOUT_FILENO

static
void print_profile(void)
{
	int i, j;
	volatile int x;

	for (i = 0; i < 100; i++) {
		x = 2;
		mm_tic();
		x /= 2;
		if (x == 1)
			x /= 10;
		mm_toc();
		for (j = 0; j < 10; j++)
			x *= 2;
		mm_toc();
		x = 2;
		for (j = 0; j < 50; j++)
			x *= 2;
		mm_toc();
	}

	mm_profile_print(PROF_MEAN|PROF_MIN|PROF_MAX, OUTFD);
	mm_profile_print(PROF_DEFAULT|PROF_FORCE_NSEC, OUTFD);
	mm_profile_print(PROF_DEFAULT|PROF_FORCE_USEC, OUTFD);
	mm_profile_print(PROF_DEFAULT|PROF_FORCE_MSEC, OUTFD);
	mm_profile_print(PROF_DEFAULT|PROF_FORCE_SEC, OUTFD);
}


static
void print_profile_labelled(void)
{
	int i, j;
	volatile int x;

	for (i = 0; i < 100; i++) {
		x = 2;
		mm_tic();
		x /= 2;
		if (x == 1)
			x /= 10;
		mm_toc_label("First step");
		for (j = 0; j < 10; j++)
			x *= 2;
		mm_toc_label("Second step");
		x = 2;
		for (j = 0; j < 50; j++)
			x *= 2;
		mm_toc_label("Last step");
	}

	mm_profile_print(PROF_MEAN|PROF_MIN|PROF_MAX, OUTFD);
}


int main(void)
{
	printf("Timing with default settings\n");
	fflush(stdout);
	print_profile();

	printf("\nTiming with wall clock timer\n");
	fflush(stdout);
	mm_profile_reset(0);
	print_profile();

	printf("\nTiming with CPU based timer\n");
	fflush(stdout);
	mm_profile_reset(PROF_RESET_CPUCLOCK);
	print_profile();

	printf("\nLabelled mm_toc() 1st\n");
	fflush(stdout);
	mm_profile_reset(PROF_RESET_CPUCLOCK);
	print_profile_labelled();

	printf("\nLabelled mm_toc() label kept\n");
	fflush(stdout);
	mm_profile_reset(PROF_RESET_CPUCLOCK|PROF_RESET_KEEPLABEL);
	print_profile_labelled();

	return EXIT_SUCCESS;
}
