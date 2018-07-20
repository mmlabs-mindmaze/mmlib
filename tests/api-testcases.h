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
TCase* create_thread_tcase(void);
TCase* create_file_tcase(void);
TCase* create_socket_tcase(void);
TCase* create_ipc_tcase(void);
TCase* create_dir_tcase(void);
TCase* create_shm_tcase(void);
TCase* create_dlfcn_tcase(void);
TCase* create_process_tcase(void);
TCase* create_argparse_tcase(void);
TCase* create_utils_tcase(void);

#endif
