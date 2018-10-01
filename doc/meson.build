install_subdir('doc/examples',
        install_dir :  get_option('datadir') / 'doc' / 'mmlib')

install_man(files('mmlog_debug.3',
    'mmlog_error.3',
    'mmlog_fatal.3',
    'mmlog_info.3',
    'mmlog_warn.3',
    'mmtic.3',
    'mmtoc.3',
    'mmtoc_label.3',
))

executable('pshared-parent-doc-example',
        files('examples/pshared-common.h', 'examples/pshared-parent.c'),
        include_directories : include_directories('.'),
        link_with : mmlib,
)

executable('pshared-child-doc-example',
        files('examples/pshared-child.c'),
        include_directories : include_directories('.'),
        link_with : mmlib,
)

executable('parse-args-doc-example',
        files('examples/parse_args.c'),
        include_directories : include_directories('.'),
        link_with : mmlib,
)
