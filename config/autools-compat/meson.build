if host_machine.system() == 'windows'
    add_project_arguments('-DEXEEXT=".exe"', language : 'c')
    config.set('LT_MODULE_EXT', '".dll"')
else
    add_project_arguments('-DEXEEXT=""', language : 'c')
    config.set('LT_MODULE_EXT', '".so"')
endif

prefix = get_option('prefix')
config.set_quoted('LOCALEDIR', prefix / get_option('localedir'))
config.set_quoted('SYSCONFDIR', prefix / get_option('sysconfdir'))
config.set_quoted('LIBEXECDIR', prefix / get_option('libexecdir') / 'mmlib')
config.set_quoted('PACKAGE_NAME', meson.project_name())
config.set_quoted('PACKAGE_VERSION', meson.project_version())
config.set_quoted('PACKAGE_STRING', meson.project_name() + ' ' + meson.project_version())
