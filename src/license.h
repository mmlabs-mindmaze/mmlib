/*
   @mindmaze_header@
*/
#ifndef LICENSE_H
#define LICENSE_H

int check_signature(const char* hwfile);
int gen_signature(const char* hwfile, const char* sigpath,
                  const char* pubkey, const char* privkey);

#endif
