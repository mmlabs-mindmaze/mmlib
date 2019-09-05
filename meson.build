project('mmlib', 'c',
        version: '1.0.0',
        license: 'apache2',
        meson_version: '>= 0.49',
        default_options: [
            'warning_level=3',
        ],
)
# follow semantic versionning (https://semver.org)
# * MAJOR version when you make incompatible API changes,
# * MINOR version when you add functionality in a backwards-compatible manner
# * PATCH version when you make backwards-compatible bug fixes.
major = '0'
minor = '3'
patch = '4'
version = major + '.' + minor + '.' + patch


cc = meson.get_compiler('c')
config = configuration_data()

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

foreach f : check_functions
    if cc.has_header_symbol(f[0], f[1])
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

if get_option('nls')
    subdir('po')
endif

subdir('src')

if get_option('tests')
    subdir('tests')
endif

if get_option('docs')
    subdir('doc')
endif

subdir('tools')
