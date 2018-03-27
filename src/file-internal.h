/*
        @mindmaze_header@
 */
#ifndef FILE_INTERNAL_H
#define FILE_INTERNAL_H


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

#endif /* FILE_INTERNAL_H */
