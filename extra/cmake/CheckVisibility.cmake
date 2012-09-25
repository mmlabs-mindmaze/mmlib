
include(CheckCSourceCompiles)

set(CMAKE_REQUIRED_FLAGS_save ${CMAKE_REQUIRED_FLAGS})
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror")
check_c_source_compiles("
         extern __attribute__((__visibility__(\"hidden\"))) int hiddenvar;
         extern __attribute__((__visibility__(\"default\"))) int exportedvar;
         extern __attribute__((__visibility__(\"hidden\"))) int hiddenfunc (void);
         extern __attribute__((__visibility__(\"default\"))) int exportedfunc (void);
         void dummyfunc (void) {}
         int main(void) {return 0;}" HAVE_VISIBILITY)
set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_save})

if(HAVE_VISIBILITY)
	add_definitions("-DLOCAL_FN=__attribute__((__visibility__(\"hidden\")))")
	add_definitions("-DAPI_EXPORTED=__attribute__((__visibility__(\"default\")))")
else(HAVE_VISIBILITY)
	add_definitions("-DLOCAL_FN")
	add_definitions("-DAPI_EXPORTED")
endif(HAVE_VISIBILITY)  
