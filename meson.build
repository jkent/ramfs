project('ramfs', 'c')

ramfs_includes = include_directories('include')
ramfs_sources = []

if get_option('use-rbtree')
    ramfs_sources += files(
        'src' / 'ramfs_rbtree.c',
        'src' / 'rbtree.c',
    )
else
    ramfs_sources += files(
        'src' / 'ramfs_vector.c',
    )
endif

libramfs = static_library('ramfs',
    ramfs_sources,
    include_directories: ramfs_includes
)

ramfs_dep = declare_dependency(
    link_with: libramfs,
    include_directories: ramfs_includes
)

meson.override_dependency('ramfs', ramfs_dep)
