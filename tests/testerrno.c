/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include <string.h>

#define print_errno_info(errnum)	\
	printf("%s (%i) : %s\n", #errnum , errnum, mmstrerror(errnum))

struct mm_error_state state_in_thread3;

static
int bar(const char* name)
{
	if (!strcmp(name, "thread3"))
		return mm_raise_error_with_extid(MM_ENOCALIB, "mmscam-calib-out", "Calibration of %s is outdated", name);

	return 0;
}

static
int foo(const char* name)
{
	if (!strcmp(name, "thread2"))
		return mm_raise_error(EINVAL, "Wrong param: bad luck");

	if (bar(name)) {
		mmlog_error("something wrong in callee of foo() in %s", name);
		return -1;
	}

	return 0;
}

static
void* thread_func(void* data)
{
	const char* thread_name = data;

	foo(thread_name);
	mm_print_lasterror("error state in %s", thread_name);

	if (!strcmp(thread_name, "thread3"))
		mm_save_errorstate(&state_in_thread3);

	return NULL;
}

int main(void)
{
	setlocale(LC_ALL, "");

	print_errno_info(MM_EDISCONNECTED);
	print_errno_info(MM_EUNKNOWNUSER);
	print_errno_info(MM_EWRONGPWD);
	print_errno_info(MM_ECAMERROR);

	pthread_t t1, t2, t3, t4;
	pthread_create(&t1, NULL, thread_func, "thread1");
	pthread_create(&t2, NULL, thread_func, "thread2");
	pthread_create(&t3, NULL, thread_func, "thread3");
	pthread_create(&t4, NULL, thread_func, "thread4");
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);
	pthread_join(t4, NULL);

	mm_set_errorstate(&state_in_thread3);
	printf("\nretrieve thread3  error state in main:\n * errnum=%i\n * in %s at %s\n * %s\n",
	       mm_get_lasterror_number(),
	       mm_get_lasterror_module(), mm_get_lasterror_location(),
	       mm_get_lasterror_desc());

	return EXIT_SUCCESS;
}
