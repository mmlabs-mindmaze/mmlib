cmake_minimum_required(VERSION 2.6)
project(mmlib)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/extra/cmake/")

include(CheckVisibility)
include(FindPthread)
if(NOT PTHREAD_FOUND)
	message(FATAL_ERROR "Cannot find pthread library")
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

