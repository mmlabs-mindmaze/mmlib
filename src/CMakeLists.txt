include_directories (${CMAKE_CURRENT_SOURCE_DIR})


add_library(mmlib SHARED log.c)
set_target_properties(mmlib PROPERTIES
                            VERSION 0.0.0
                            SOVERSION 0)
target_link_libraries(mmlib ${PTHREAD_LIBS})

install(TARGETS mmlib 
        LIBRARY DESTINATION lib)

install(FILES mmlog.h DESTINATION include)

