/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmsysio.h"
#include "mmerrno.h"

#include <sys/mman.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


/**************************************************************************
 *                                                                        *
 *                     tracking of block of mapping                       *
 *                                                                        *
 **************************************************************************/
#define MIN_NUM_ENTRIES		64

/**
 * struct map_entry - memory map entry
 * @ptr:        starting address of mapping
 * @len:        length of mapping
 */
struct map_entry {
	void* ptr;
	size_t len;
};


/**
 * struct mapping_list - list keeping track of effective memory mapping
 * @entries:            array of map entries
 * @num_entries:        number of memory mapping
 * @num_max_entries:    length of @entries array
 * @mtx:                mutex protecting modification of list
 */
struct mapping_list {
	struct map_entry* entries;
	int num_entries;
	int num_max_entries;
	pthread_mutex_t mtx;
};


/**
 * mapping_list_cleanup() - cleanup memory mapping list
 * @list:       mapping list to cleanup
 *
 * Typically called when program exit (or library unload) to frees data used
 * to keep track of memory mapping.
 */
static
void mapping_list_cleanup(struct mapping_list* list)
{
	free(list->entries);

	list->num_entries = 0;
	list->num_max_entries = 0;
}


/**
 * mapping_list_add_entry() - register a new map entry in mapping list
 * @list:       mapping list to modify
 * @ptr:        starting address of mapping
 * @len:        length of mapping
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 */
static
int mapping_list_add_entry(struct mapping_list* list, void* ptr, size_t len)
{
	int retval = 0;
	int num_max;
	struct map_entry* entries;
	struct map_entry* entry;

	pthread_mutex_lock(&list->mtx);

	entries = list->entries;
	num_max = list->num_max_entries;

	// Resize entries array if too small
	if (num_max <= list->num_entries) {
		if (num_max < MIN_NUM_ENTRIES)
			num_max = MIN_NUM_ENTRIES;
		else
			num_max *= 2;

		entries = realloc(entries, num_max*sizeof(*entries));
		if (!entries) {
			mm_raise_from_errno("Can't allocate mapping list");
			retval = -1;
			goto exit;
		}

		list->entries = entries;
		list->num_max_entries = num_max;
	}

	// Add mapping block to the mapping list
	entry = &entries[list->num_entries++];
	entry->ptr = ptr;
	entry->len = len;

exit:
	pthread_mutex_unlock(&list->mtx);

	return retval;
}


/**
 * mapping_list_add_entry() - unregister a map entry from mapping list
 * @list:       mapping list to modify
 * @ptr:        starting address of mapping to remove
 *
 * Return: A non negative integer corresponding to the size of the mapping
 * in case of success, -1 otherwise with error state set
 */
static
ssize_t mapping_list_remove_entry(struct mapping_list* list, void* ptr)
{
	int i;
	bool found;
	size_t mapping_size = -1;
	struct map_entry* entries;

	pthread_mutex_lock(&list->mtx);

	entries = list->entries;

	// Search the mapping in the entries
	found = false;
	for (i = 0; i < list->num_entries; i++) {
		if (ptr == entries[i].ptr) {
			found = true;
			break;
		}
	}

	// Check the mapping has been found
	if (!found)
		goto exit;

	// Remove mapping for list
	mapping_size = entries[i].len;
	memmove(entries + i, entries + i+1,
	        (list->num_entries-i-1)*sizeof(*entries));
	list->num_entries--;

exit:
	pthread_mutex_unlock(&list->mtx);

	if (!found) {
		mm_raise_error(EFAULT, "Address do no refer to any mapping");
		return -1;
	}
	return mapping_size;
}


static struct mapping_list file_mapping_list = {
	.mtx = PTHREAD_MUTEX_INITIALIZER
};


MM_DESTRUCTOR(mapping_cleanup)
{
	mapping_list_cleanup(&file_mapping_list);
}


/**
 * mapblock_add() - register a memory mapping in global mapping list
 * @ptr:        starting address of mapping
 * @len:        length of mapping
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 */
static
int mapblock_add(void* ptr, size_t len)
{
	return mapping_list_add_entry(&file_mapping_list, ptr, len);
}


/**
 * mapblock_remove() - Remove a mapping from global mapping list
 * @ptr:        starting address of mapping to remove
 *
 * Return: A non negative integer corresponding to the size of the mapping
 * in case of success, -1 otherwise with error state set
 */
static
ssize_t mapblock_remove(void* ptr)
{
	return mapping_list_remove_entry(&file_mapping_list, ptr);
}

/**************************************************************************
 *                                                                        *
 *                File mapping and shared memory object APIs              *
 *                                                                        *
 **************************************************************************/

/**
 * get_mmap_prot() - get the mmap memory protection of the mapping
 * @mflags:     flags passed to mm_mapfile
 *
 * Return: the memory protection value suitable for mmap()
 */
static
int get_mmap_prot(int mflags)
{
	int prot = PROT_NONE;

	if (mflags & MM_MAP_READ)
		prot |= PROT_READ;

	if (mflags & MM_MAP_WRITE)
		prot |= PROT_WRITE;

	if (mflags & MM_MAP_EXEC)
		prot |= PROT_EXEC;

	return prot;
}


/**
 * get_mmap_flags() - get the mmap flags
 * @mflags:     flags passed to mm_mapfile
 *
 * Return: visibility flags for mmap()
 */
static
int get_mmap_flags(int mflags)
{
	int flags = 0;

	if (mflags & MM_MAP_SHARED)
		flags |= MAP_SHARED;

	return flags;
}


/**
 * mm_mapfile() - map pages of memory
 * @fd:         file descriptor of file to map in memory
 * @offset:     offset within the file from which the mapping must start
 * @len:        length of the mapping
 * @mflags:     control how the mapping is done
 *
 * The mm_mapfile() function establishes a mapping between a process'
 * address space and a portion or the entirety of a file or shared memory
 * object represented by @fd. The portion of the object to map can be
 * controlled by the parameters @offset and @len. @offset must be a multiple
 * of page size.
 *
 * The flags in parameters @mflags determines whether read, write, execute,
 * or some combination of accesses are permitted to the data being mapped.
 * The requested access can of course cannot grant more permission than the
 * one associated with @fd.
 *
 * MM_MAP_READ
 *   Data can be read
 * MM_MAP_WRITE
 *   Data can be written
 * MM_MAP_EXEC
 *   Data can be executed
 * MM_MAP_SHARED
 *   Change to mapping are shared
 *
 * If MM_MAP_SHARED is specified, write change the underlying object.
 * Otherwise, modifications to the mapped data by the calling process will
 * be visible only to the calling process and shall not change the
 * underlying object.
 *
 * The mm_mapfile() function adds an extra reference to the file associated
 * with the file descriptor @fd which is not removed by a subsequent
 * mm_close() on that file descriptor. This reference will be removed when
 * there are no more mappings to the file.
 *
 * Return: The starting address of the mapping in case of success.
 * Otherwise NULL is returned and error state is set accordingly.
 */
API_EXPORTED
void* mm_mapfile(int fd, mm_off_t offset, size_t len, int mflags)
{
	int prot, flags;
	void* addr;

	prot = get_mmap_prot(mflags);
	flags = get_mmap_flags(mflags);

	addr = mmap(NULL, len, prot, flags, fd, offset);
	if (addr == MAP_FAILED) {
		mm_raise_from_errno("mmap failed");
		return NULL;
	}

	// Register memory mapping
	if (mapblock_add(addr, len)) {
		munmap(addr, len);
		return NULL;
	}

	return addr;
}


/**
 * mm_unmap() - unmap pages of memory
 * @addr:       starting address of memory block to unmap
 *
 * Remove a memory mapping previously established. @addr must be NULL or must
 * have been returned by a successful call to mm_mapfile(). If @addr is NULL,
 * mm_unmap() do nothing.
 *
 * Return: 0 in case of success, -1 otherwise with error state set.
 */
API_EXPORTED
int mm_unmap(void* addr)
{
	ssize_t len;

	if (!addr)
		return 0;

	// Get memory mapping, retrieve its size and forget it.
	len = mapblock_remove(addr);
	if (len < 0)
		return -1;

	if (munmap(addr, len)) {
		mm_raise_from_errno("munmap failed");
		return -1;
	}
	return 0;
}


API_EXPORTED
int mm_anon_shm(void)
{
	static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
	int fd;
	char name[32];

	sprintf(name, "/mmlib-shm-%u", getpid());

	pthread_mutex_lock(&mtx);
	fd = shm_open(name, O_RDWR|O_CREAT|O_EXCL, 0);
	if (fd != -1)
		shm_unlink(name);
	pthread_mutex_unlock(&mtx);

	if (fd == -1)
		return mm_raise_from_errno("anon_shm() failed");
	return fd;
}


API_EXPORTED
int mm_shm_open(const char* name, int oflag, int mode)
{
	int fd;

	fd = shm_open(name, oflag, mode);
	if (fd == -1)
		return mm_raise_from_errno("shm_open(%s, ...) failed", name);

	return fd;
}


API_EXPORTED
int mm_shm_unlink(const char* name)
{
	if (shm_unlink(name))
		return mm_raise_from_errno("shm_unlink(%s) failed", name);

	return 0;
}
