/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <fileapi.h>
#include <synchapi.h>

#include <uchar.h>

#include "mmerrno.h"
#include "mmpredefs.h"
#include "mmsysio.h"
#include "utils-win32.h"
#include "volume-win32.h"


#define INVALID_VOLUME_DEV (~((mm_dev_t)0))

static SRWLOCK volume_lock = SRWLOCK_INIT;
static int num_cached_volumes = 0;
static struct volume cached_volumes[32];


/*
 * This descructor cleans up at exit, the volumes information cached lazily by
 * the function get_volume().
 */
MM_DESTRUCTOR(volume_cache)
{
	int i;

	for (i = 0; i < num_cached_volumes; i++)
		free(cached_volumes[i].guid_path);
}


static
HANDLE open_volume_path(char16_t* volume_path)
{
	DWORD access = READ_CONTROL;
	DWORD create = OPEN_EXISTING;
	DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
	DWORD share = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;

	return CreateFileW(volume_path, access, share, NULL, create, flags, NULL);
}


static
mm_dev_t get_volume_dev(HANDLE hnd)
{
	FILE_ID_INFO id_info = {.VolumeSerialNumber = INVALID_VOLUME_DEV};

	get_file_id_info_from_handle(hnd, &id_info);

	return id_info.VolumeSerialNumber;
}


static
int get_volume_fs_type(HANDLE hnd)
{
	char16_t fs_name[32] = {0};

	if (!GetVolumeInformationByHandleW(hnd, NULL, 0, NULL, NULL, NULL,
	                                   fs_name, sizeof(fs_name)-1))
		return FSTYPE_UNKNOWN;

	if (!wcscmp(fs_name, L"NTFS"))
		return FSTYPE_NTFS;
	if (!wcscmp(fs_name, L"FAT32"))
		return FSTYPE_FAT32;
	if (!wcscmp(fs_name, L"EXFAT"))
		return FSTYPE_EXFAT;

	return FSTYPE_UNKNOWN;
}


static
void volume_init(struct volume* vol, const char16_t* volume_path,
		 mm_dev_t dev, HANDLE hnd)
{
	vol->dev = dev;
	vol->guid_path = wcsdup(volume_path);
	vol->fs_type = get_volume_fs_type(hnd);
}


static
struct volume* search_in_cached_volumes(mm_dev_t dev)
{
	int i;

	for (i = 0; i < num_cached_volumes; i++) {
		if (dev == cached_volumes[i].dev)
			return &cached_volumes[i];
	}

	return NULL;
}


static
const struct volume* add_volume(mm_dev_t dev)
{
	HANDLE hnd, find_vol_hnd;
	char16_t root[MAX_PATH];
	struct volume* vol = NULL;

	if (num_cached_volumes == MM_NELEM(cached_volumes)) {
		mm_raise_error(EOVERFLOW, "Too many volumes in cache");
		return NULL;
	}

	root[0] = L'\0';

	find_vol_hnd = FindFirstVolumeW(root, MAX_PATH);
	if (find_vol_hnd == INVALID_HANDLE_VALUE) {
		mm_raise_from_w32err("FindFirstVolume failed");
		return NULL;
	}

	// Loop over volume on system until one has a matching serial
	do {
		hnd = open_volume_path(root);
		if (hnd == INVALID_HANDLE_VALUE)
			continue;

		if (dev == get_volume_dev(hnd)) {
			// We have a match
			vol = &cached_volumes[num_cached_volumes++];
			volume_init(vol, root, dev, hnd);
		}

		CloseHandle(hnd);
	} while (!vol && FindNextVolumeW(find_vol_hnd, root, MAX_PATH));

	FindVolumeClose(find_vol_hnd);

	if (vol == NULL)
		mm_raise_error(ENODEV, "Could not find volume %016llx", dev);

	return vol;
}


LOCAL_SYMBOL
const struct volume* get_volume_from_dev(mm_dev_t dev)
{
	const struct volume* vol;

	AcquireSRWLockExclusive(&volume_lock);

	vol = search_in_cached_volumes(dev);
	if (!vol)
		vol = add_volume(dev);

	ReleaseSRWLockExclusive(&volume_lock);

	return vol;
}
