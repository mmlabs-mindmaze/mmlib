project('mmlib', 'c',
        version: '1.2.0',
        license: 'apache2',
        meson_version: '>= 0.49',
        default_options: [
            'c_std=c11',
            'warning_level=3',
        ],
)


cc = meson.get_compiler('c')
config = configuration_data()

# additional (optional) warnings
flags = [
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wmissing-prototypes',

    # (clang) we know how C works
    '-Wno-string-plus-int'
]
add_project_arguments(cc.get_supported_arguments(flags), language : 'c')

# use POSIX definitions up to POSIX.1-2008 standard
config.set('_POSIX_C_SOURCE', '200809L')

# ensure that the "default" definitions are provided
# Note: when defined with _POSIX_C_SOURCE, POSIX sources are always preferred
# in case of conflict
config.set('_DEFAULT_SOURCE', '1')
config.set('_BSD_SOURCE', '1')  # for compatibility with old OS

subdir('config/api-exports')
subdir('config/autools-compat')

if host_machine.system() == 'windows'
    # windows-specifig config
    subdir('config/windows')
endif

# header checks
check_headers = [
	'dlfcn.h',
	'locale.h',
]
foreach h : check_headers
    cc.check_header(h)
endforeach

# non-mandatory function checks
check_functions = [
	['sys/mman.h', 'mmap'],
	['stdlib.h', 'posix_memalign'],
	['stdlib.h', 'aligned_alloc'],
    ['malloc.h', '_aligned_malloc'],
	['malloc.h', '_aligned_free'],
	['dlfcn.h', 'dlopen'],
	['pthread.h', 'pthread_mutex_consistent'],
]

# Note: do not use cc.has_function() here: it uses the compiler builtins to
# answer. BUT sometimes, the compiler lies ... for example gcc lies on
# mingw64 and has a builtin advertising for posix_memalign().
foreach f : check_functions
    if cc.has_header_symbol(f[0], f[1], args:'-D_POSIX_C_SOURCE=200809L')
        config.set('HAVE_' + f[1].underscorify().to_upper(), 1)
    endif
endforeach

configuration_inc = include_directories('.', 'src')

# write config file
build_cfg = 'config.h'  # named as such to match autotools build system
configure_file(output : build_cfg, configuration : config)

# define HAVE_CONFIG_H with compiler command line to include the generated
# config.h file (same as autotools)
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')

nls_state = 'disabled'
gettext = find_program('gettext', required: get_option('nls'))
if gettext.found() and not get_option('nls').disabled()
    nls_state = 'enabled'
    subdir('po')
endif

subdir('src')

tests_state = 'disabled'
libcheck = dependency('check', required : get_option('tests'))
if libcheck.found() and not get_option('tests').disabled()
    tests_state = 'enabled'

    cc.check_header('check.h')
    subdir('tests')
endif

docs_state = 'disabled'
sphinxbuild = find_program('sphinx-build', required : get_option('docs'))
if sphinxbuild.found() and not get_option('docs').disabled()
    # TODO
    # meson does not yet know how to check for a python module presence
    # (introduced in 0.51.0)
    # change this when possible
    python3 = import('python').find_installation('python3', required : true)
    check_linuxdoc = run_command(python3, '-c', '"import linuxdoc"')

    if check_linuxdoc.returncode() != 0 and get_option('docs') == 'enabled'
        error('python3 module "linuxdoc" is required to build documentation')
    elif check_linuxdoc.returncode() == 0
        docs_state = 'enabled'
    endif
endif

if docs_state == 'enabled'
    subdir('doc')
endif

subdir('tools')

# print the status of the auto-detect features
message('Configuration summary')
message('=====================')
message('* Tests : @0@'.format(tests_state))
message('* Doc building : @0@'.format(docs_state))
message('* NLS usage : @0@'.format(nls_state))
