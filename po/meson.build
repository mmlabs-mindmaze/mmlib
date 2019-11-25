# use meson internationalization module, ie that generates/updates po and pot
# files
i18n = import('i18n')
i18n.gettext(meson.project_name(),
    po_dir: 'po',
    args: [
         '--keyword=_', '--keyword=N_',
         '--copyright-holder=Mindmaze SA',
         '--package-name=' + meson.project_name(),
         '--package-version=' + meson.project_version(),
         '--msgid-bugs-address=nicolas.bourdaud@mindmaze.ch',
    ]
)
