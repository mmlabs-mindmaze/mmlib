/*
   @mindmaze_header@
*/
#ifndef PSHARED_LOCK_H
#define PSHARED_LOCK_H

#include <windows.h>
#include <stdint.h>
#include "lock-referee-proto.h"

/**
 * struct lockref_connection - data handling connection to lock referee server
 * @pipe:       handle to a client end of the named pipe to lock server
 * @robust_data: pointer to the memory map of the robust data
 * @is_init:    1 if the structure is initialized, 0 otherwise. If it is not
 *              initialized, the other fields in the structure hold
 *              meaningless value.
 */
struct lockref_connection {
	HANDLE pipe;
	struct robust_data* robust_data;
	int is_init;
};

/**
 * struct shared_lock - description of a process shared lock
 * @key:        key of the lock known by lock server
 * @ptr:        pointer to the lock value shared by all lock users (points
 *              likely to memory mapped data if associated mutex or
 *              condition is shared with other processes).
 */
struct shared_lock {
	int64_t key;
	int64_t* ptr;
};


/**
 * struct lock_timeout - data indicating a timeout for a lock
 * @clk_flags:	flag indicating the clock type to use for timeout. Must be
 *              one of the WAITCLK_FLAG_* flag value
 * @ts:         absolute time of the timeout, clock base is specified by @clk_flags
 */
struct lock_timeout {
	int clk_flags;
	struct timespec ts;
};

void deinit_lock_referee_connection(struct lockref_connection* conn);
struct robust_data* pshared_get_robust_data(struct lockref_connection* conn);
int64_t pshared_init_lock(struct lockref_connection* conn);
int pshared_wait_on_lock(struct lockref_connection* conn, struct shared_lock lock,
                         int64_t wakeup_val, const struct lock_timeout* timeout);
void pshared_wake_lock(struct lockref_connection* conn, struct shared_lock lock,
                       int64_t val, int num_wakeup);

#endif
