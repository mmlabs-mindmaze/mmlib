/*
 *      @mindmaze_header@
 */
#ifndef ERROR_INTERNAL_H
#define ERROR_INTERNAL_H

struct error_info {
	int flags;              // flags to finetune error handling
	int errnum;             // error class (standard and mmlib errno value)
	char extended_id[64];   // message to display to end user if has not
	                        // been caught before
	char module[32];        // module that has generated the error
	char location[256];     // which function/file/line has generated the
	                        // error
	char desc[256];         // message intended to developer
};


struct error_info* get_thread_last_error(void);


#endif /* ERROR_INTERNAL_H */
