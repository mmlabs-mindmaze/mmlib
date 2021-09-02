install_subdir('examples',
        install_dir :  get_option('datadir') / 'doc' / 'mmlib')

install_man(files('mm_log_debug.3',
    'mm_log_error.3',
    'mm_log_fatal.3',
    'mm_log_info.3',
    'mm_log_warn.3',
    'mm_tic.3',
    'mm_toc.3',
    'mm_toc_label.3',
))

pshared_parent_doc_example_sources = files(
        'examples/pshared-common.h',
        'examples/pshared-parent.c'
)
executable('pshared-parent-doc-example',
        pshared_parent_doc_example_sources,
        include_directories : include_directories('../src'),
        link_with : mmlib,
)

pshared_child_doc_example_sources = files('examples/pshared-child.c')
executable('pshared-child-doc-example',
        pshared_child_doc_example_sources,
        include_directories : include_directories('../src'),
        link_with : mmlib,
)

parse_args_doc_example = files('examples/parse_args.c')
executable('parse-args-doc-example',
        parse_args_doc_example,
        include_directories : include_directories('../src'),
        link_with : mmlib,
)

sphinxbuild = find_program('sphinx-build', required : get_option('docs'))
if sphinxbuild.found()
    # TODO
    # meson does not yet know how to check for a python module presence
    # (introduced in 0.51.0)
    # change this when possible
    python3 = import('python').find_installation('python3', required : true)
    check_linuxdoc = run_command(python3, '-c', '"import linuxdoc"')
    if check_linuxdoc.returncode() != 0
        error('python3 module "linuxdoc" is required too build documentation')
    endif

    doc_sources = files(
            'alloc.rst',
            'argparse.rst',
            'design.rst',
            'dlfcn.rst',
            'env.rst',
            'error.rst',
            'examples.rst',
            'filesystem.rst',
            'index.rst',
            'ipc.rst',
            'log.rst',
            'process.rst',
            'profiling.rst',
            'socket.rst',
            'thread.rst',
            'time.rst',
    )

    # sphinx warning: do NOT reorder the flags !
    # sphinx-build NEEDS the flags -M and -d both first and in this order !
    sphinxbuild_wrapper = files('sphinx-build-wrapper.sh')
    gen_man_pages = custom_target('man3',
            output : 'man3',
            command : [
                'sh',
                sphinxbuild_wrapper,
                sphinxbuild,
                meson.project_source_root(),
                'kernel-doc-man',
                meson.project_source_root() / 'doc',
                'doc/man3',
            ],
            build_by_default : true,
            depend_files : [mmlib_sources, doc_sources],
            install : true,
            install_dir : get_option('mandir'),
    )

    custom_target('html',
            output : 'html',
            command : [
                'sh',
                sphinxbuild_wrapper,
                sphinxbuild,
                meson.project_source_root(),
                'html',
                meson.project_source_root() / 'doc',
                'doc/html',
            ],
            build_by_default : true,
            depend_files : [mmlib_sources, doc_sources],
            depends : gen_man_pages,  # re-use .doctree from man
            install : true,
            install_dir : get_option('datadir') / 'doc/mmlib',
    )

endif # sphinxbuild
