if host_machine.system() == 'windows'
    add_project_arguments('-DEXEEXT=".exe"', language : 'c')
    config.set('LT_MODULE_EXT', '".dll"')
else
    add_project_arguments('-DEXEEXT=""', language : 'c')
    config.set('LT_MODULE_EXT', '".so"')
endif

config.set('LOCALEDIR', get_option('localedir'))
config.set('SYSCONFDIR', get_option('sysconfdir'))
config.set('LIBEXECDIR', '"' + get_option('libexecdir') + '/mmlib"')
config.set('PACKAGE_NAME', '"' + meson.project_name() + '"')
config.set('PACKAGE_VERSION', '"' + meson.project_version() + '"')
config.set('PACKAGE_STRING', '"' + meson.project_name() + ' ' + meson.project_version() + '"')
