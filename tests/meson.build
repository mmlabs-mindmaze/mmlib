cc.check_header('check.h')
libcheck = dependency('check', required : true)
# '-DCHECK_SUPPORT_TAP=1',
unittest_args = [
    '-DSRCDIR="' + meson.source_root() + '"',
    '-DBUILDDIR="' + meson.current_build_dir() + '"',
    '-DTOP_BUILDDIR="' + meson.build_root() + '"',
    '-DLT_OBJDIR="."',
]


testlog = executable('testlog',
        files('testlog.c'),
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)
test('testlog', testlog)

testerrno = executable('testerrno',
        files('testerrno.c'),
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)
test('testerrno', testerrno)

testprofile = executable('testprofile',
        files('testprofile.c'),
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)
test('testprofile', testprofile)


executable('child-proc',
        files('child-proc.c'),
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)

executable('tests-child-proc',
        files('tests-child-proc.c',
            'tests-child-proc.h',
            'process-testlib.c',
            'process-testlib.h',
            'threaddata-manipulation.h',
            'threaddata-manipulation.c',
            'socket-testlib.h',
            'socket-testlib.c',
            'ipc-api-tests-exported.c',
            'ipc-api-tests-exported.h',
        ),
        c_args : unittest_args + ['-DMMLOG_MODULE_NAME="tests_child_proc"'],
        export_dynamic : true,
        include_directories : configuration_inc,
        link_with : mmlib,
)

perflock = executable('perflock',
        files('perflock.c'),
        include_directories : configuration_inc,
        c_args : unittest_args,
        link_with : mmlib,
        dependencies: [libcheck],
)

shared_module('dynlib-test',
        files('dynlib-api.h', 'dynlib-test.c'),
        name_prefix : '',  # do not prefix with 'lib'
        include_directories : configuration_inc,
        c_args : unittest_args,
)

# TODO: testinternals

testapi_sources = files(
        'alloc-api-tests.c',
        'api-testcases.h',
        'argparse-api-tests.c',
        'dirtests.c',
        'dlfcn-api-tests.c',
        'file-api-tests.c',
        'geometry-api-tests.c',
        'ipc-api-tests.c',
        'ipc-api-tests-exported.c',
        'ipc-api-tests-exported.h',
        'process-api-tests.c',
        'shm-api-tests.c',
        'socket-api-tests.c',
        'socket-testlib.c',
        'socket-testlib.h',
        'testapi.c',
        'tests-child-proc.h',
        'tests-run-func.c',
        'thread-api-tests.c',
        'threaddata-manipulation.c',
        'threaddata-manipulation.h',
        'time-api-tests.c',
        'type-api-tests.c',
        'utils-api-tests.c'
)

testapi_deps = [libcheck]
if host_machine.system() == 'windows'
    testapi_deps += libws2_32
endif
testapi = executable('testapi',
        testapi_sources,
        c_args : unittest_args + cflags,
        include_directories : configuration_inc,
        link_with : mmlib,
        dependencies: testapi_deps,
)

# increase timeout to 5 min (300s) for windows
# as long as *all* the API tests are part of the same program, it makes
# sense anyway
test('unit api tests', testapi,
        timeout : 300,
)
