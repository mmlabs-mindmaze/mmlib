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
#include <mmthread.h>
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
	int rv;
	char buffer[512];

	setlocale(LC_ALL, "");

	print_errno_info(MM_EDISCONNECTED);
	print_errno_info(MM_EUNKNOWNUSER);
	print_errno_info(MM_EWRONGPWD);
	print_errno_info(MM_ECAMERROR);

	mmthread_t t1, t2, t3, t4;
	mmthr_create(&t1, thread_func, "thread1");
	mmthr_create(&t2, thread_func, "thread2");
	mmthr_create(&t3, thread_func, "thread3");
	mmthr_create(&t4, thread_func, "thread4");
	mmthr_join(t1, NULL);
	mmthr_join(t2, NULL);
	mmthr_join(t3, NULL);
	mmthr_join(t4, NULL);

	mm_set_errorstate(&state_in_thread3);
	printf("\nretrieve thread3  error state in main:\n * errnum=%i\n extid=%s * in %s at %s\n * %s\n",
			mm_get_lasterror_number(),
			mm_get_lasterror_module(),
			mm_get_lasterror_extid(),
			mm_get_lasterror_location(),
			mm_get_lasterror_desc());

	rv = mmstrerror_r(mm_get_lasterror_number(), buffer, sizeof(buffer));
	printf("\n%s to get lase error msg from number: %s\n",
			rv == 0 ? "Succeeded" : "Failed",
			buffer);

	return EXIT_SUCCESS;
}
