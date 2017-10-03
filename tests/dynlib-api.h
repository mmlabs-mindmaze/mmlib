/*
   @mindmaze_header@
*/
#ifndef DYNLIB_API_H
#define DYNLIB_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define INITIAL_INTVAL  0xdeadbeef
#define INITIAL_STR     "str is init"

struct dynlib_data {
	int intval;
	char str[32];
};

struct dynlib_vtab {
	void (*set_data)(int, const char*);
	void (*reset_data)(void);
	int (*read_internal_code)(void);
	void (*set_internal_code)(int);
};


#ifdef __cplusplus
}
#endif

#endif
