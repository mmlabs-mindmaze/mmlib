# shared library needed for the tests of rename and unlink
lib_handmaid = shared_library('handmaid',
        files('lib_handmaid.c', 'lib_handmaid.h'),
        install : true,
        include_directories : configuration_inc,
        version : '1.0.0',
)

# executable needed for the tests of rename and unlink
executable('handmaid',
        'handmaid.c',
        install : true,
        include_directories : configuration_inc,
        link_with : [lib_handmaid, mmlib],
)
