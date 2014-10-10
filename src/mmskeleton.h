/*
   @mindmaze_header@
*/
#ifndef MMSKELETON_H
#define MMSKELETON_H

#ifdef __cplusplus
extern "C" {
#endif

/* GCC 2.95 and later have "__restrict"; C99 compilers have
   "restrict", and "configure" may have defined "restrict".  */
#if ! (199901L <= __STDC_VERSION__ || defined restrict)
# if 2 < __GNUC__ || (2 == __GNUC__ && 95 <= __GNUC_MINOR__) \
     || defined __restrict
#  define restrict __restrict
# else
#  define restrict
# endif
#endif

struct mmbone {
	float rot[4];	// rotation in reference frame
	float pos[3];   // joint position, ie, point of rotation of the bone
	                // relative to parent frame
	short child;
	short brother;
};

struct mmskel {
	struct mmbone* bones;	// array of bones (first is root)
	int nbone;
	int cachelen;
	char* strcache;		// pool of memory holding all the strings
	int* name_idx;		// string indices of joint names
};

int skl_init(struct mmskel* sk);
void skl_deinit(struct mmskel* sk);
int skl_parentlist(const struct mmskel* sk, int* parent);
int skl_find(const struct mmskel* restrict sk, const char* name);
int skl_add(struct mmskel* sk, int par, const char* name);
int skl_add_to(struct mmskel* sk, const char* parent, const char* name);
int skl_load_data(struct mmskel* skel, int fd);
int skl_save_data(struct mmskel* skel, int fd);

// Joint ids used
#define MSK_VL5 "vl5"
#define MSK_VT1 "vt1"
#define MSK_SKB "skb"
#define MSK_SKT "skt"
#define MSK_LSHO "lsho"
#define MSK_LELB "lelb"
#define MSK_LWRI "lwri"
#define MSK_LTH1 "lth1"
#define MSK_LTHT "ltht"
#define MSK_LMID1 "lmid1"
#define MSK_LMIDT "lmidt"
#define MSK_RSHO "rsho"
#define MSK_RELB "relb"
#define MSK_RWRI "rwri"
#define MSK_RTH1 "rth1"
#define MSK_RTHT "rtht"
#define MSK_LMID1 "lmid1"
#define MSK_LMIDT "lmidt"

#ifdef __cplusplus
}
#endif

#endif /* MMSKELETON_H */

