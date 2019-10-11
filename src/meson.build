public_headers = files(
        'mmargparse.h',
        'mmdlfcn.h',
        'mmerrno.h',
        'mmgeometry.h',
        'mmlib.h',
        'mmlog.h',
        'mmpredefs.h',
        'mmprofile.h',
        'mmskeleton.h',
        'mmsysio.h',
        'mmthread.h',
        'mmtime.h',
        'mmtype.h',
)
install_headers(public_headers)

mmlib_sources = files(
        'alloc.c',
        'argparse.c',
        'dlfcn.c',
        'error.c',
        'file.c',
        'file-internal.h',
        'geometry.c',
        'log.c',
        'mmargparse.h',
        'mmdlfcn.h',
        'mmerrno.h',
        'mmgeometry.h',
        'mmlib.h',
        'mmlog.h',
        'mmprofile.h',
        'mmskeleton.h',
        'mmsysio.h',
        'mmthread.h',
        'mmtime.h',
        'mmtype.h',
        'nls-internals.h',
        'profile.c',
        'skeleton.c',
        'socket.c',
        'time.c',
        'type.c',
        'utils.c',
)

libm = cc.find_library('m', required : true)
cflags = []
dependencies = [libm]

if get_option('nls')
    cflags += '-DENABLE_NLS'
    if not cc.has_function('ngettext')
        dependencies += cc.find_library('intl', required : true)
    endif
endif

if host_machine.system() == 'windows'
    mmlib_sources += files(
        'atomic-win32.h',
        'clock-win32.c',
        'clock-win32.h',
        'env-win32.c',
        'file-win32.c',
        'local-ipc-win32.c',
        'lock-referee-proto.h',
        'mutex-lockval.h',
        'process-win32.c',
        'pshared-lock.c',
        'pshared-lock.h',
        'shm-win32.c',
        'socket-win32.c',
        'startup-win32.c',
        'thread-win32.c',
        'time-win32.c',
        'utils-win32.c',
        'utils-win32.h',
        'volume-win32.c',
        'volume-win32.h',
    )

    libpowrprof = cc.find_library('powrprof', required : true)
    libws2_32 = cc.find_library('ws2_32', required : true)

    cflags += [
        '-DMMLIB_API=API_EXPORTED',
        '-D__USE_MINGW_ANSI_STDIO=1',
    ]
    dependencies += [libpowrprof, libws2_32]
else
    mmlib_sources += files(
        'file-posix.c',
        'local-ipc-posix.c',
        'process-posix.c',
        'shm-posix.c',
        'socket-posix.c',
        'thread-posix.c',
        'time-posix.c',
    )

    libthread = dependency('threads', required : true)
    libdl = cc.find_library('dl', required : true)
    librt = cc.find_library('rt', required : true)

    dependencies += [libthread, libdl, librt]
endif

lock_referee_sources = []
if host_machine.system() == 'windows'
    if get_option('lock-server-process')
        lock_referee_sources += files(
                'lock-referee-proto.h',
                'lock-referee-server.c',
                'clock-win32.h',
                'clock-win32.c',
        )

        libadvapi32 = cc.find_library('advapi32', required : true)

        executable('lock-referee',
                lock_referee_sources,
                c_args : cflags + ['-DMMLOG_MODULE_NAME="lock-referee"'],
                include_directories : configuration_inc,
                install : true,
                install_dir : get_option('libexecdir'),
                dependencies : [libpowrprof, libadvapi32],
        )
    else
        add_project_arguments('-DLOCKSERVER_IN_MMLIB_DLL=1', language : 'c')
        mmlib_sources += files('lock-referee-server.c')
    endif # lock-server-process
endif # windows


# follow semantic versionning
# https://semver.org/
# * MAJOR version when you make incompatible API changes,
# * MINOR version when you add functionality in a backwards-compatible manner
# * PATCH version when you make backwards-compatible bug fixes.

major = '0'
minor = '4'
patch = '0'
version = major + '.' + minor + '.' + patch

mmlib_static = static_library('mmlib-static',
        mmlib_sources,
        c_args : cflags,
        install : false,
        include_directories : configuration_inc,
        dependencies : [dependencies],
)
mmlib = shared_library('mmlib',
        mmlib_sources,
        c_args : cflags,
        install : true,
        version : version,
        include_directories : configuration_inc,
        dependencies : [dependencies],
)
import('pkgconfig').generate(mmlib)
