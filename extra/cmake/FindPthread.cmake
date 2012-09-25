
include(CheckCSourceCompiles)

set(CMAKE_REQUIRED_LIBRARIES_save ${CMAKE_REQUIRED_LIBRARIES})
set(PTHREAD_PROGTEST "#include <pthread.h>
                      #include <stddef.h>
		      void* dummy(void* data) {(void)data; return NULL;}
                      int main(void) {
		      pthread_t th;
		      pthread_create(&th, NULL, dummy, NULL);
                      return 0;}")

set(PTHREAD_LIBS -lpthread)
set(CMAKE_REQUIRED_LIBRARIES "${PTHREAD_LIBS}")
check_c_source_compiles("${PTHREAD_PROGTEST}" PTHREAD_FOUND)
if (NOT PTHREAD_FOUND)
	unset(PTHREAD_LIBS)
endif()

set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_save})
unset(CMAKE_REQUIRED_LIBRARIES_save)
unset(PTHREAD_PROGTEST)
