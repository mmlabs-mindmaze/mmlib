/*
   @mindmaze_header@
*/

/* multithreaded data write
 *
 * example of child process, to be used by pshared-parent.c example.
 *
 * This program writes to a shared text (in shared memory) concurrently with
 * other threads in the same process. When notified, a worker thread tries
 * to write its identification string ("|-thread-X+|") onto a text field of
 * the shared memory. The text after update of several worker threads looks
 * something like:
 *
 *     ...|+thread-Z+||+thread-W+||+thread-X+||+thread-Y+|...
 *
 * This file demonstrates how to:
 *  - map file into memory
 *  - use process shared mutex
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mmthread.h>

#define NUM_THREAD	6
#define MAX_ID_LEN	16

struct shared_data {
	mmthr_mtx_t mutex;
	int len;
	char text[1024];
	mmthr_mtx_t notif_mtx;
	mmthr_cond_t notif_cond;
	int start;
};

struct thread_data {
	struct shared_data* shdata;
	char id_str[MAX_ID_LEN];
};


/*
 * This function do the update of the shared text. It happens the
 * string |+@id_str+| to the text field in @psh_data.
 */
static
void write_shared_data(struct shared_data* shdata, const char* id_str)
{
	int id_str_len = strlen(id_str);

	// Get the shared lock. Since we are using a normal mutex, we do not
	// have to check the return value
	mmthr_mtx_lock(&shdata->mutex);

	// Add "|+" in the text
	shdata->text[shdata->len++] = '|';
	shdata->text[shdata->len++] = '+';

	// Append process identifier on text
	memcpy(shdata->text + shdata->len, id_str, id_str_len);
	shdata->len += id_str_len;

	// Add "+|" in the text
	shdata->text[shdata->len++] = '+';
	shdata->text[shdata->len++] = '|';

	mmthr_mtx_unlock(&shdata->mutex);
}


static
void wait_start_notification(struct shared_data* shdata)
{
	mmthr_mtx_lock(&shdata->notif_mtx);

	// A while loop is necessary, because a spurious wakeup is always
	// possible
	while (!shdata->start)
		mmthr_cond_wait(&shdata->notif_cond, &shdata->notif_mtx);

	mmthr_mtx_unlock(&shdata->notif_mtx);
}


static
void broadcast_start_notification(struct shared_data* shdata)
{
	// We want a worker thread to be be scheduled in a predictable way,
	// so we must own shdata->notif_mtx when calling
	// mmthr_cond_broadcast()
	mmthr_mtx_lock(&shdata->notif_mtx);

	shdata->start = 1;
	mmthr_cond_broadcast(&shdata->notif_cond);

	mmthr_mtx_unlock(&shdata->notif_mtx);
}


static
void* thread_func(void* data)
{
	struct thread_data* thdata = data;
	struct shared_data* shdata = thdata->shdata;
	const char* id_str = thdata->id_str;

	// Put a wait here to force a litle bit of more contention. This is
	// here only for demonstration purpose... Without it, since the
	// update of text is short and simple, the text would be likely
	// filed in the order of thread creation
	wait_start_notification(shdata);

	write_shared_data(shdata, id_str);

	return NULL;
}


int main(void)
{
	int i;
	mmthread_t thid[NUM_THREAD];
	struct thread_data thdata[NUM_THREAD];
	struct shared_data shared = {
		.mutex = MMTHR_MTX_INITIALIZER,
		.notif_mtx = MMTHR_MTX_INITIALIZER,
		.notif_cond = MMTHR_COND_INITIALIZER,
		.start = 0,
	};

	// Create threads and assign each an ID string
	for (i = 0; i < NUM_THREAD; i++) {
		thdata[i].shdata = &shared;
		sprintf(thdata[i].id_str, "thread-%i", i);
		mmthr_create(&thid[i], thread_func, &thdata[i]);
	}

	// Now that all thread are created, we can signal them to start
	broadcast_start_notification(&shared);

	for (i = 0; i < NUM_THREAD; i++)
		mmthr_join(thid[i], NULL);

	printf("result string:%s\n", shared.text);
	return EXIT_SUCCESS;
}

