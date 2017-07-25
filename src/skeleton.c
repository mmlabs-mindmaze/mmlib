/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>

#include "mmskeleton.h"
#include "mmlog.h"
#include "mmerrno.h"


struct locale_store {
	locale_t prevloc;
	locale_t newloc;
};

static
int push_default_locale(struct locale_store* store)
{
	locale_t loc, prev_loc;

	loc = newlocale(LC_NUMERIC_MASK, "POSIX", (locale_t)0);
	if (!loc)
		return -1;

	prev_loc = uselocale(loc);
	if (!prev_loc)
		return -1;

	store->prevloc = prev_loc;
	store->newloc = loc;
	return 0;
}

static
void pop_locale(struct locale_store* store)
{
	uselocale(store->prevloc);
	if (store->newloc != (locale_t)0)
		freelocale(store->newloc);
}

static
const char skel_magic_number[] = {'%', 'M', 'M', 'S', 'K', 'E', 'L', '0'};

static
int bone_dfs(const struct mmskel* skel, int cur, int par, void* funcdata,
              int (bone_func)(const struct mmskel*, int, int, void*))
{
	if (bone_func(skel, cur, par, funcdata))
		return -1;

	par = cur;
	cur = skel->bones[par].child;
	while (cur != -1) {
		if (bone_dfs(skel, cur, par, funcdata, bone_func))
			return -1;
		cur = skel->bones[cur].brother;
	}
	return 0;
}


static
int set_parent_iter(const struct mmskel* skel, int c, int par, void* data)
{
	(void)skel;
	int* parent = data;
	parent[c] = par;
	return 0;
}


static
const char* get_joint_name(const struct mmskel* restrict skel, int boneind)
{
	if (boneind >= skel->nbone || boneind < 0) {
		mm_raise_error(MM_ENOTFOUND, "Joint %i not in the admissible range [0-%i]", boneind, skel->nbone);
		return NULL;
	}

	return skel->strcache + skel->name_idx[boneind];
}


static
int save_bone_data(const struct mmskel* skel, int c, int par, void* data)
{
	int wsize;
	const char *bone, *parent;
	const float* pos = skel->bones[c].pos;
	FILE* fp = data;

	bone = get_joint_name(skel, c);
	parent = (par >= 0) ? get_joint_name(skel, par) : "NULL";

	wsize = fprintf(fp, "\n|%s|%s|%f|%f|%f|",
	                    parent, bone, pos[0], pos[1], pos[2]);
	if (wsize < 0) {
		mm_raise_error(errno, "cannot write bone line: %s", strerror(errno));
		return -1;
	}
	return 0;
}


API_EXPORTED
int skl_find(const struct mmskel* restrict skel, const char* name)
{
	int i;
	
	for (i = 0; i < skel->nbone; i++) {
		if (!strcmp(name, get_joint_name(skel, i)))
			return i;
	}

	mm_raise_error(MM_ENOTFOUND, "Cannot find joint %s in skeleton", name);
	return -1;
}



API_EXPORTED
int skl_add(struct mmskel* skel, int par, const char* jname)
{
	char* strcache = skel->strcache;
	struct mmbone *bones = skel->bones;
	int *name_idx = skel->name_idx;
	int alloc_nbone = skel->nbone;
	int alloc_cachelen = skel->cachelen;
	short *next;
	int ind;

	// Resize graph memory
	alloc_cachelen += strlen(jname) + 1;
	alloc_nbone += 1;
	strcache = realloc(strcache, alloc_cachelen);
	bones = realloc(skel->bones, alloc_nbone*sizeof(*bones));
	name_idx = realloc(name_idx, alloc_nbone*sizeof(*name_idx));
	if (!bones || !strcache || !name_idx) {
		free((strcache != skel->strcache) ? strcache : NULL);
		free((bones != skel->bones) ? bones : NULL);
		free((name_idx != skel->name_idx) ? name_idx : NULL);
		mm_raise_error(errno, "Cannot resize skeleton internal buffers");
		return -1;
	}

	// Happen the new bone
	ind = skel->nbone;
	bones[ind].child = -1;
	bones[ind].brother = -1;
	name_idx[ind] = skel->cachelen;
	strcpy(strcache + name_idx[ind], jname);

	// Connect the new bone to the graph (update parent or brother)
	if (par != -1) {
		next = &(bones[par].child);
		while (*next != -1)
			next = &(bones[*next].brother);
		*next = ind;
	}

	// Update skeleton
	skel->nbone++;
	skel->bones = bones;
	skel->strcache = strcache;
	skel->cachelen = alloc_cachelen;
	skel->name_idx = name_idx;

	return 0;
}


API_EXPORTED
int skl_add_to(struct mmskel* skel, const char* parent, const char* name)
{
	int par = parent ? skl_find(skel, parent) : -1;
	
	// Check that a bone parent has been found when one is specified
	if (parent && par == -1) {
		return -1;
	}

	return skl_add(skel, par, name);
}


API_EXPORTED
int skl_parentlist(const struct mmskel* sk, int* parent)
{
	if (!sk || !parent) {
		mm_raise_error(EINVAL, "Missing argument");
		return -1;
	}

	bone_dfs(sk, 0, -1, parent, set_parent_iter);
	return 0;
}


API_EXPORTED
void skl_deinit(struct mmskel* skel)
{
	if (!skel)
		return;

	free(skel->bones);
	free(skel->name_idx);
	free(skel->strcache);

	skel->bones = NULL;
	skel->name_idx = NULL;
	skel->strcache = NULL;
	skel->nbone = 0;
	skel->cachelen = 0;
}


API_EXPORTED
int skl_init(struct mmskel* skel)
{
	skel->bones = NULL;
	skel->name_idx = NULL;
	skel->strcache = NULL;
	skel->nbone = 0;
	skel->cachelen = 0;

	return 0;
}


/**
 * skl_load_data() - Read skeleton from a file descriptor
 * @skel:       Pointer to the skeleton data
 * @fd:         File descriptor to load the data from
 *
 * Load the skeleton data from a file descriptor. Since the source is
 * specified by a file descriptor, this function can be used to load data
 * possibly through IPC. To avoid any issues, the skeleton are reinitialized
 * at the beginning of the function. Thus it is not necessary to call
 * skl_init() beforehand.
 *
 * In order to avoid bad loading of the data, it is performed
 * with POSIX locale set. The previous locale is of course restored before
 * leaving the function.
 *
 * Returns: 0 in case of success, -1 otherwise and error state is set
 * accordingly
 */
API_EXPORTED
int skl_load_data(struct mmskel* skel, int fd)
{
	char magic[sizeof(skel_magic_number)];
	char bone[32], parent[32];
	float pos[3];
	int nf, ind, root, newfd = -1, ret = -1;
	FILE* fp = NULL;
	struct locale_store locstore = {0};

	if (!skel) {
		mm_raise_error(EINVAL, "Skeleton argument not set");
		return -1;
	}
	skl_init(skel);

	if ((newfd = dup(fd)) == -1
	  || fcntl(newfd, F_SETFL, fcntl(newfd, F_GETFL) | FD_CLOEXEC)
	  || push_default_locale(&locstore)
	  || !(fp = fdopen(newfd, "r")) )
		goto exit;

	// Check file format
	if ( (nf = fread(magic, sizeof(magic), 1, fp)) < 1
	    || memcmp(magic, skel_magic_number, sizeof(magic))) {
		if (nf == 1)
			errno = MM_EBADFMT;
		goto exit;
	}

	// Loop over the file and add each bone
	while (!feof(fp)) {
		nf = fscanf(fp, " |%31[^|]|%32[^|]|%f|%f|%f| ",
		                parent, bone, &pos[0], &pos[1], &pos[2]);
		if (nf < 5) {
			if (!ferror(fp))
				errno = MM_EBADFMT;
			goto exit;
		}

		root = !strcmp(parent, "NULL");
		if (skl_add_to(skel, root ? NULL : parent, bone))
			goto exit;

		ind = skl_find(skel, bone);
		memcpy(skel->bones[ind].pos, pos, sizeof(pos));
	}

	ret = 0;

exit:
	if (ret)
		mm_raise_error(errno, "skl_load_data() failed: %s", mmstrerror(errno));

	if (fp)
		fclose(fp);
	else if (newfd != -1)
		close(newfd);

	pop_locale(&locstore);

	return ret;
}


/**
 * skl_save_data() - Save skeleton on a file descriptor
 * @skel:       Pointer to the skeleton data
 * @fd:         File descriptor to write the data to
 *
 * Save the skeleton data on a file descriptor. Since the target is
 * specified by a file descriptor, this function can be used to save data
 * possibly through IPC.
 *
 * In order to avoid bad representation in data, the loading is performed
 * with POSIX locale set.  The previous locale is of course restored before
 * leaving the function.
 *
 * Returns: 0 in case of success, -1 otherwise and error state is set
 * accordingly
 */
API_EXPORTED
int skl_save_data(struct mmskel* skel, int fd)
{
	int nf, newfd = -1, ret = -1;
	FILE* fp = NULL;
	struct locale_store locstore = {0};

	if (!skel) {
		mm_raise_error(EINVAL, "Skeleton argument not set");
		return -1;
	}

	if ((newfd = dup(fd)) == -1
	  || fcntl(newfd, F_SETFL, fcntl(newfd, F_GETFL) | FD_CLOEXEC)
	  || push_default_locale(&locstore)
	  || !(fp = fdopen(newfd, "w")) ) {
		mm_raise_error(errno, "Cannot change locale or use fd - %s", mmstrerror(errno));
		goto exit;
	}

	// Write file format magic number
	nf = fwrite(skel_magic_number, sizeof(skel_magic_number), 1, fp);
	if (nf != 1) {
		mm_raise_error(errno, "Cannot write magic number - %s", mmstrerror(errno));
		goto exit;
	}

	// Search the graph (depth first search)
	// and write data for each node
	if (bone_dfs(skel, 0, -1, fp, save_bone_data))
		goto exit;

	ret = 0;

exit:
	if (fp)
		fclose(fp);
	else if (newfd != -1)
		close(newfd);

	pop_locale(&locstore);

	return ret;
}
