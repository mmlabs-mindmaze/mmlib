/*
   @mindmaze_header@
*/
#ifndef API_TESTCASES_H
#define API_TESTCASES_H

#include <check.h>

TCase* create_type_tcase(void);
TCase* create_geometry_tcase(void);
TCase* create_allocation_tcase(void);
TCase* create_time_tcase(void);

#endif
