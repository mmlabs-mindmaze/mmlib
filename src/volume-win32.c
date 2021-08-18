/*
 * @mindmaze_header@
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

	return CreateFileW(volume_path, access, FILE_SHARE_ALL, NULL, create,
	                   flags, NULL);
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


/**
 * volume_get_trash_prefix_u16() - Get folder of user recycle bin or alike
 * @vol:        volume in which the trash must be found
 * @path:       output buffer (must be at least MAX_PATH char16_t wide)
 *
 * Return: In case of success, the length of prefix written in @path in term of
 * number of char16_t (excluding string terminator). Otherwise, -1 is returned.
 * Please note that this function does not set error state. Use GetLastError()
 * to retrieve the origin of error.
 */
LOCAL_SYMBOL
int volume_get_trash_prefix_u16(const struct volume* vol, char16_t* path)
{
	const char16_t* sid;

	// On FAT (FAT32 and exFAT), there aren't any recycle bin folder (at
	// least in many Windows version) but those filesystem do not have
	// permission per file, hence we can always write in the root folder of
	// the volume (if writable volume).
	if (vol->fs_type == FSTYPE_FAT32 || vol->fs_type == FSTYPE_EXFAT) {
		return swprintf(path, MAX_PATH, L"%ls\\", vol->guid_path);
	}

	sid = get_caller_string_sid_u16();
	if (!sid)
		return -1;

	// We use recycle bin of the same volume because this is a writable
	// folder available on each NTFS volume. Now for the FS not NTFS and
	// not FAT we don't have workaround. Hence we prefer hoping there is a
	// $Recycle.Bin... We will fallback to usual windows behavior if not.
	return swprintf(path, MAX_PATH, L"%ls\\$Recycle.bin\\%ls\\",
	                vol->guid_path, sid);
}
