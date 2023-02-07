/*
 *      @mindmaze_header@
 */
#ifndef FILE_INTERNAL_H
#define FILE_INTERNAL_H


static inline
int is_path_separator(char c)
{
#if defined (_WIN32)
	return (c == '\\' || c == '/');
#else
	return (c == '/');
#endif
}


/* use to skip "." and ".." directories */
static inline
int is_wildcard_directory(const char * name)
{
	int len;

	for (len = 0; (len <= 2) && (name[len] != '\0'); len++) {
		if (name[len] != '.')
			return 0;
	}

	return (len == 1 || len == 2);
}


int copy_internal(const char* src, const char* dst, int flags, int mode);
int internal_mkdir(const char* path, int mode);



#endif /* FILE_INTERNAL_H */
