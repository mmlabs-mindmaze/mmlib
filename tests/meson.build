unittest_args = [
    '-DCHECK_SUPPORT_TAP=1',
    '-DSRCDIR="' + meson.source_root() + '"',
    '-DBUILDDIR="' + meson.current_build_dir() + '"',
    '-DTOP_BUILDDIR="' + meson.build_root() + '"',
    '-DLT_OBJDIR="."',
]

testlog_sources = files('testlog.c')
testlog = executable('testlog',
        testlog_sources,
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)
test('testlog', testlog)

testerrno_sources = files('testerrno.c')
testerrno = executable('testerrno',
        testerrno_sources,
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)
test('testerrno', testerrno)

testprofile_sources = files('testprofile.c')
testprofile = executable('testprofile',
        testprofile_sources,
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)
test('testprofile', testprofile)

child_proc_sources = files('child-proc.c')
executable('child-proc',
        child_proc_sources,
        c_args : unittest_args,
        include_directories : configuration_inc,
        link_with : mmlib,
)

tests_child_proc_files = files('tests-child-proc.c',
        'tests-child-proc.h',
        'process-testlib.c',
        'process-testlib.h',
        'threaddata-manipulation.h',
        'threaddata-manipulation.c',
        'socket-testlib.h',
        'socket-testlib.c',
        'ipc-api-tests-exported.c',
        'ipc-api-tests-exported.h',
)
executable('tests-child-proc',
        tests_child_proc_files,
        c_args : unittest_args + ['-DMMLOG_MODULE_NAME="tests_child_proc"'],
        export_dynamic : true,
        include_directories : configuration_inc,
        link_with : mmlib,
)

perflock_sources = files('perflock.c')
perflock = executable('perflock',
        perflock_sources,
        include_directories : configuration_inc,
        c_args : unittest_args,
        link_with : mmlib,
        dependencies: [libcheck],
)

dynlib_test_sources = files('dynlib-api.h', 'dynlib-test.c')
shared_module('dynlib-test',
        dynlib_test_sources,
        name_prefix : '',  # do not prefix with 'lib'
        include_directories : configuration_inc,
        c_args : unittest_args,
)

test_internals_sources = files(
        'internals-testcases.h',
        'log-internals.c',
        'testinternals.c',
)
if host_machine.system() == 'windows'
    test_internals_sources += files('startup-win32-internals.c')
endif

testinternals = executable('testinternals',
        test_internals_sources,
        c_args : unittest_args + cflags,
        include_directories : configuration_inc,
        link_with : mmlib_static,
        dependencies: libcheck,
)

test('internal tests', testinternals,
     env: ['CK_VERBOSITY=silent'],
     protocol: 'tap',
)

# create the binary and shared library for the rename and unlink tests
subdir('handmaid')

testapi_sources = files(
        'alloc-api-tests.c',
        'api-testcases.h',
        'argparse-api-tests.c',
        'dirtests.c',
        'dlfcn-api-tests.c',
        'file_advanced_tests.c',
        'file-api-tests.c',
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
        'utils-api-tests.c'
)

libmath = cc.find_library('m', required : true)
testapi_deps = [libcheck, libmath]
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
     env: ['CK_VERBOSITY=silent'],
     timeout : 300,
     protocol: 'tap',
)
