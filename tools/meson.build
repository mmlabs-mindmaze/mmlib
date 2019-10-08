all_lib_c_sources = mmlib_sources + lock_referee_sources

if get_option('tests')
    all_test_c_sources = (testlog_sources
            + testerrno_sources
            + testprofile_sources
            + child_proc_sources
            + tests_child_proc_files
            + perflock_sources
            + dynlib_test_sources
            + testapi_sources
    )
else
    all_test_c_sources = []
endif

if get_option('docs')
    all_doc_c_sources = (pshared_parent_doc_example_sources
            + pshared_child_doc_example_sources
            + parse_args_doc_example
    )
else
    all_doc_c_sources = []
endif

all_sources = (all_lib_c_sources
        + all_test_c_sources
        + all_doc_c_sources
)

codespell = find_program('codespell', required : false)
if codespell.found()
    # check for typo in all possible sources
    run_target('spelling',
        command : [
            codespell,
            all_sources,
        ]
    )
endif # codespell


uncrustify = find_program('uncrustify', required : false)
if uncrustify.found()
    uncrustify_config = files('uncrustify.cfg')

    # enforce coding style for library sources and documentation
    # leave the test sources out of this constraint

    run_target('checkstyle',
        command : [
            uncrustify,
            '-l', 'c',
            '-c', uncrustify_config,
            '--check',
            all_lib_c_sources, all_doc_c_sources
        ],
    )
    run_target('fixstyle',
        command : [
            uncrustify,
            '-l', 'c',
            '-c', uncrustify_config,
            '--replace',
            all_lib_c_sources, all_doc_c_sources
        ],
    )
endif # uncrustify

# test public headers for most warnings and C++ compatibility
api_compat_test = run_target('api-compat-test',
        command : [
            files('api-compat-test.sh'),
            public_headers,
        ]
)
