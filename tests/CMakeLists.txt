include_directories (${CMAKE_CURRENT_SOURCE_DIR}/../src)


add_executable(testlog testlog.c)
target_link_libraries(testlog mmlib)
add_test(testlog ${CMAKE_CURRENT_BINARY_DIR}/testlog)
add_definitions(-DPACKAGE_NAME=\"${PROJECT_NAME}\")
