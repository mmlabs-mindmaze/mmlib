/*
   @mindmaze_header@
*/

/* process shared data: child program
 *
 * example of child process, to be used by pshared-parent.c example.
 *
 * Similar to the multithreaded data write example, this program writes to a
 * shared text (in shared memory) concurrently to other children of the
 * parent process. Each child maps into memory a file descriptor
 * (SHM_CHILD_FD) inherited from parent and initialized there.  The child
 * tries to write its identification string ("|-child-X+|") onto a text
 * field of the shared memory. The text after update of several child looks
 * something like:
 *
 *     ...|+child-Z+||+child-W+||+child-X+||+child-Y+|...
 *
 * Because of the concurent access, the children use a process shared mutex
 * mapped in the shared memory. They can recover from a child dying while
 * owning the mutex. Put simulate this, the SEGFAULT_IN_CHILD environment
 * variable can be set. If a child process see its identification string
 * ("child-X" for Xth child created), it will provoke a segfault while
 * updating the text.
 *
 * This file demonstrates how to:
 *  - map file into memory
 *  - use process shared mutex
 */

#define MMLOG_MODULE_NAME "pshared-child"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <mmthread.h>
#include <mmsysio.h>
#include <mmerrno.h>
#include <mmlib.h>

#include "pshared-common.h"

#define BAD_ADDR	(void*)0xDEADBEEF


static
void handle_notif_lock_retval(int lockret, struct pshared_data* psh_data)
{
	// By far the most usual case. We simply got the lock, nothing fancy
	// has happened.
	if (lockret == 0)
		return;

	// Contrary to psh_data->mutex, there is no shared state to recover
	// with psh_data->notif_mtx. Simply mark it consistent
	if (lockret == EOWNERDEAD)
		mmthr_mtx_consistent(&psh_data->notif_mtx);

	if (lockret == ENOTRECOVERABLE)
		exit(EXIT_FAILURE);
}


/*
 * Wait that parent notifies to start, ie, mark psh_data->start = 1 and
 * broadcast psh_data->notif_cond.
 */
static
void wait_start_notification(struct pshared_data* psh_data)
{
	int lockret;

	lockret = mmthr_mtx_lock(&psh_data->notif_mtx);
	handle_notif_lock_retval(lockret, psh_data);

	while (!psh_data->start) {
		lockret = mmthr_cond_wait(&psh_data->notif_cond, &psh_data->notif_mtx);
		handle_notif_lock_retval(lockret, psh_data);
	}

	mmthr_mtx_unlock(&psh_data->notif_mtx);
}


/*
 * This function do the update of the shared text. It must be called while
 * holding the lock, ie, called from write_shared_data(). It happens the
 * string |+@id_str+| to the text field in @psh_data.
 *
 * If requested, this function will provoke a segfault in the middle of
 * string appending.
 */
static
void write_shared_text_locked(struct pshared_data* psh_data, const char* id_str,
                              bool provoke_segfault)
{
	int id_str_len = strlen(id_str);

	// Add "|+" in the text
	psh_data->text[psh_data->len++] = '|';
	psh_data->text[psh_data->len++] = '+';

	// Append process identifier on text (psh_data->len not updated yet)
	memcpy(psh_data->text + psh_data->len, id_str, id_str_len);

	// Segfaulting here is a good place for demonstration purpose:
	// psh_data->len will be not consistent with the null-terminated
	// string in psh_data->text
	if (provoke_segfault)
		strcpy(psh_data->text + psh_data->len, BAD_ADDR);

	// Now update psh_data->len
	psh_data->len += id_str_len;

	// Add "+|" in the text
	psh_data->text[psh_data->len++] = '+';
	psh_data->text[psh_data->len++] = '|';
}


/*
 * Function to recover shared state from the situation where the previous
 * owner died while holding the lock.
 */
static
void recover_shared_text_from_owner_dead(struct pshared_data* psh_data)
{
	int len = psh_data->len;
	char* text = psh_data->text;

	// Find index of text immediately after the last occurence of "+|"
	while (len > 0) {
		if ((len > 2) && (text[len-2]=='+') && (text[len-1]=='|'))
			break;

		len--;
	}

	// Crop string and set to the proper found length
	text[len] = '\0';
	psh_data->len = len;
}



static
void write_shared_data(struct pshared_data* psh_data, const char* id_str,
                       bool provoke_segfault)
{
	int r;

	// Get the shared lock. Since we are using a process shared mutex,
	// we must check return value of the lock operation: If the previous
	// owner has died while owning it, it will be only occasion to know
	// about it and recover from this if we want to continue using it.
	r = mmthr_mtx_lock(&psh_data->mutex);
	if (r == EOWNERDEAD) {
		// We have the lock, but it is time to recover state since
		// previous owner died while holding the lock
		recover_shared_text_from_owner_dead(psh_data);

		// We have recovered the shared state, so we can mark lock as
		// consistent. After this, we will be back to normal operation
		mmthr_mtx_consistent(&psh_data->mutex);
	} else if (r == ENOTRECOVERABLE) {
		// There has been an lock owner that has died and the next
		// owner failed (or refused) to mark lock as consistent,
		// thus rendering the lock unusable. This provokes all
		// waiters for the lock to be waken up and ENOTRECOVERABLE
		// is returned. Any new attempt to lock the mutex wil return
		// ENOTRECOVERABLE (until it is deinit and init again).

		// So now, we don't have the lock and we can only stop
		return;
	}

	write_shared_text_locked(psh_data, id_str, provoke_segfault);

	mmthr_mtx_unlock(&psh_data->mutex);
}


int main(int argc, char* argv[])
{
	int mflags;
	bool must_segfault;
	struct pshared_data* psh_data = NULL;
	const char* proc_string;

	// identifier of process is passed in the first argument
	if (argc < 2) {
		fprintf(stderr, "%s is missing argument\n", argv[0]);
		return EXIT_FAILURE;
	}

	proc_string = argv[1];

	// Map shared memory object onto memory.  We know that child is
	// created with shared memrory file descriptor inherited at
	// SHM_CHILD_FD
	mflags = MM_MAP_SHARED|MM_MAP_READ|MM_MAP_WRITE;
	psh_data = mm_mapfile(SHM_CHILD_FD, 0, sizeof(*psh_data), mflags);
	if (!psh_data) {
		mm_print_lasterror("mm_mapfile(%i, ...) failed", SHM_CHILD_FD);
		return EXIT_FAILURE;
	}

	// Close SHM_CHILD_FD because now that it is mapped, we don't need
	// it any longer
	mm_close(SHM_CHILD_FD);

	// Get from environment if this particular instance must simulate a
	// segfault while holding the lock.
	must_segfault = false;
	if (!strcmp(mm_getenv("SEGFAULT_IN_CHILD", ""), proc_string))
		must_segfault = true;

	// Wait until parent notify to start
	wait_start_notification(psh_data);

	// Try to update shared text.
	write_shared_data(psh_data, proc_string, must_segfault);

	mm_unmap(psh_data);
	return EXIT_SUCCESS;
}
