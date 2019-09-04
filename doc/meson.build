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

pshared_parent_doc_example_sources = files(
        'examples/pshared-common.h',
        'examples/pshared-parent.c'
)
executable('pshared-parent-doc-example',
        pshared_parent_doc_example_sources,
        include_directories : include_directories('.'),
        link_with : mmlib,
)

pshared_child_doc_example_sources = files('examples/pshared-child.c')
executable('pshared-child-doc-example',
        pshared_child_doc_example_sources,
        include_directories : include_directories('.'),
        link_with : mmlib,
)

parse_args_doc_example = files('examples/parse_args.c')
executable('parse-args-doc-example',
        parse_args_doc_example,
        include_directories : include_directories('.'),
        link_with : mmlib,
)
