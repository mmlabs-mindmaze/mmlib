/*
	Copyright (C) 2013  MindMaze SA
	All right reserved

	Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "mmskeleton.h"
#include "mmlog.h"
#include "mmerrno.h"

static
void bone_dfs(const struct mmskel* skel, int cur, int par, void* funcdata,
              void (bone_func)(const struct mmskel*, int, int, void*))
{
	bone_func(skel, cur, par, funcdata);

	par = cur;
	cur = skel->bones[par].child;
	while (cur != -1) {
		bone_dfs(skel, cur, par, funcdata, bone_func);
		cur = skel->bones[cur].brother;
	}
}


static
void set_parent_iter(const struct mmskel* skel, int c, int par, void* data)
{
	(void)skel;
	int* parent = data;
	parent[c] = par;
}


static
const char* get_joint_name(const struct mmskel* restrict skel, int boneind)
{
	if (boneind >= skel->nbone || boneind < 0)
		return NULL;

	return skel->strcache + skel->name_idx[boneind];
}


API_EXPORTED
int skl_find(const struct mmskel* restrict skel, const char* name)
{
	int i;
	
	for (i = 0; i < skel->nbone; i++) {
		if (!strcmp(name, get_joint_name(skel, i)))
			return i;
	}

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
		errno = MM_ENOTFOUND;
		return -1;
	}

	return skl_add(skel, par, name);
}


API_EXPORTED
int skl_parentlist(const struct mmskel* sk, int* parent)
{
	if (!sk || !parent)
		return -1;

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

