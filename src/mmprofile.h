/*
   @mindmaze_header@
*/
#ifndef MMPROFILE_H
#define MMPROFILE_H

#define PROF_CURR	0x01
#define PROF_MIN	0x02
#define PROF_MAX	0x04
#define PROF_MEAN	0x08

#ifdef __cplusplus
extern "C" {
#endif

void mmtic(void);
void mmtoc(void);
int mmprofile_print(int mask, int fd);
void mmprofile_reset(int cputime);

#ifdef __cplusplus
}
#endif

#endif
