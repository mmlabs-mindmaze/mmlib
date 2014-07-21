/*
   @mindmaze_header@
*/
#ifndef MMPROFILE_H
#define MMPROFILE_H

#define PROF_CURR	0x01
#define PROF_MIN	0x02
#define PROF_MAX	0x04
#define PROF_MEAN	0x08

#define PROF_RESET_CPUCLOCK     0x01
#define PROF_RESET_KEEPLABEL    0x02

#ifdef __cplusplus
extern "C" {
#endif

void mmtic(void);
void mmtoc(void);
void mmtoc_label(const char* label);
int mmprofile_print(int mask, int fd);
void mmprofile_reset(int reset_flags);

#ifdef __cplusplus
}
#endif

#endif
