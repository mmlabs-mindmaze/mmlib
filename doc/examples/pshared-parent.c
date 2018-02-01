/*
   @mindmaze_header@
*/

/* process shared data: parent program
 *
 * example of parent process, child is implemented in pshared-child.c.
 *
 * This program writes to a shared text (in shared memory) concurrently to
 * other children of the parent process. Each child maps into memory a file
 * descriptor (SHM_CHILD_FD) inherited from parent and initialized there.
 * The child tries to write its identification string ("|-child-X+|") onto a
 * text field of the shared memory. The text after update of several child
 * looks something like:
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
 * Note on how to execute the program:
 * If left untouched, the program assumes that child executable is available
 * in the _current_ directory. Also it assumes that mmlib shared library is
 * accessible at runtime.
 *
 * This file demonstrates how to:
 *  - create an anonymous shared memory object
 *  - map file into memory
 *  - initialize process shared mutex
 *  - create child process with passing file descriptor to them
 */
#define MMLOG_MODULE_NAME "pshared-parent"

#include <stdlib.h>
#include <stdio.h>
#include <mmthread.h>
#include <mmsysio.h>
#include <mmpredefs.h>
#include <mmerrno.h>
#include <mmlib.h>

#include "pshared-common.h"


#ifdef _WIN32
#  define       BINEXT  ".exe"
#else
#  define       BINEXT
#endif

#define NUM_CHILD	6
#define PSHARED_CHILD_BIN	"./pshared-child" BINEXT

/*
 * Create, map into memory and initialize the data that will shared with the
 * children. The process shared mutex is initialized here.
 */
static
struct pshared_data* init_shared_mem_data(int* shm_fd)
{
	int fd, mflags;
	struct pshared_data* psh_data = NULL;

	// Create an new anonymous shared memory object. We could have use a
	// normal file (with mm_open()) without changing of the rest of the
	// following if we wanted to keep the result of memory access on the
	// shared memory.
	fd = mm_anon_shm();
	if (fd < 0)
		return NULL;

	// Size it to accomodate the data that will be shared between
	// parent and children.
	if (mm_ftruncate(fd, sizeof(*psh_data)))
		goto failure;

	// Map shared memory object onto memory
	mflags = MM_MAP_SHARED|MM_MAP_READ|MM_MAP_WRITE;
	psh_data = mm_mapfile(fd, 0, sizeof(*psh_data), mflags);
	if (!psh_data)
		goto failure;

	// Reset the while content of sturcture to 0/NULL fields
	*psh_data = (struct pshared_data){.start = 0};

	// Initialize synchronization primitives of shared data
	if ( mmthr_mtx_init(&psh_data->mutex, MMTHR_PSHARED)
	  || mmthr_mtx_init(&psh_data->notif_mtx, MMTHR_PSHARED)
	  || mmthr_cond_init(&psh_data->notif_cond, MMTHR_PSHARED) )
		goto failure;

	*shm_fd = fd;
	return psh_data;

failure:
	mm_close(fd);
	return NULL;
}


/*
 * Starts all children process ensuring that they inherit of the shared
 * memory file descriptor. Pass the string identify a particular process
 * instance as the first argument.
 */
static
int spawn_children(int shm_fd, int num_child, mm_pid_t* childs)
{
	int i;
	char process_identifier[32];
	char* argv[] = {PSHARED_CHILD_BIN, process_identifier, NULL};
	struct mm_remap_fd fd_map = {
		.child_fd = SHM_CHILD_FD,
		.parent_fd = shm_fd,
	};

	for (i = 0; i < num_child; i++) {
		// Set the process identifier (it is just a string to
		// identify which child process is running). This string is
		// already set as second element in argv, ie, the first
		// argument
		sprintf(process_identifier, "child-%i", i);

		// Spawn the process
		if (mm_spawn(&childs[i], argv[0], 1, &fd_map, 0, argv, NULL))
			return -1;
	}

	return 0;
}


static
int wait_children_termination(int num_child, const mm_pid_t* childs)
{
	int i;

	for (i = 0; i < num_child; i++) {
		if (mm_wait_process(childs[i], NULL))
			return -1;
	}

	return 0;
}


static
void broadcast_start_notification(struct pshared_data* psh_data)
{
	int lockret;

	// We want a worker thread to be be scheduled in a predictable way,
	// so we must own shdata->notif_mtx when calling
	// mmthr_cond_broadcast()
	lockret = mmthr_mtx_lock(&psh_data->notif_mtx);
	if (lockret == ENOTRECOVERABLE)
		return;
	if (lockret == EOWNERDEAD)
		mmthr_mtx_consistent(&psh_data->notif_mtx);

	psh_data->start = 1;
	mmthr_cond_broadcast(&psh_data->notif_cond);

	mmthr_mtx_unlock(&psh_data->notif_mtx);
}


int main(void)
{
	mm_pid_t childs[NUM_CHILD];
	int shm_fd = -1;
	struct pshared_data* psh_data = NULL;
	int exitcode = EXIT_FAILURE;

	fprintf(stderr, "SEGFAULT_IN_CHILD=%s\n", mm_getenv("SEGFAULT_IN_CHILD", ""));

	// Create a shared memory object with the right size and map into
	// memory
	psh_data = init_shared_mem_data(&shm_fd);
	if (!psh_data)
		goto exit;

	// Create the children inheriting the shared memory object
	if (spawn_children(shm_fd, MM_NELEM(childs), childs))
		goto exit;

	// Close shm_fd because now that it is mapped, and transmitted to
	// children, we don't need its file descriptor.
	mm_close(shm_fd);
	shm_fd = -1;

	broadcast_start_notification(psh_data);

	wait_children_termination(MM_NELEM(childs), childs);
	exitcode = EXIT_SUCCESS;

exit:
	if (exitcode == EXIT_FAILURE)
		mm_print_lasterror("pshared-parent failed");
	else
		printf("result string:\%s\n", psh_data->text);

	mm_close(shm_fd);
	mm_unmap(psh_data);
	return exitcode;
}
