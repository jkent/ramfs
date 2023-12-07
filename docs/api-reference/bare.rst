Bare API
========

`ramfs/ramfs.h`

Functions
^^^^^^^^^

.. doxygenfunction:: ramfs_init
.. doxygenfunction:: ramfs_deinit
.. doxygenfunction:: ramfs_get_parent
.. doxygenfunction:: ramfs_get_entry
.. doxygenfunction:: ramfs_get_name
.. doxygenfunction:: ramfs_get_path
.. doxygenfunction:: ramfs_is_dir
.. doxygenfunction:: ramfs_is_file
.. doxygenfunction:: ramfs_stat
.. doxygenfunction:: ramfs_create
.. doxygenfunction:: ramfs_truncate
.. doxygenfunction:: ramfs_open
.. doxygenfunction:: ramfs_close
.. doxygenfunction:: ramfs_read
.. doxygenfunction:: ramfs_write
.. doxygenfunction:: ramfs_seek
.. doxygenfunction:: ramfs_tell
.. doxygenfunction:: ramfs_access
.. doxygenfunction:: ramfs_unlink
.. doxygenfunction:: ramfs_rename
.. doxygenfunction:: ramfs_opendir
.. doxygenfunction:: ramfs_closedir
.. doxygenfunction:: ramfs_readdir
.. doxygenfunction:: ramfs_seekdir
.. doxygenfunction:: ramfs_telldir
.. doxygenfunction:: ramfs_mkdir
.. doxygenfunction:: ramfs_rmdir
.. doxygenfunction:: ramfs_rmtree

Enums
^^^^^

.. doxygenenum:: ramfs_entry_type_t

Typedefs
^^^^^^^^

.. doxygentypedef:: ramfs_fs_t
.. doxygentypedef:: ramfs_entry_t

Structs
^^^^^^^

.. doxygenstruct:: ramfs_stat_t
    :members:
.. doxygenstruct:: ramfs_dh_t
    :members:
.. doxygenstruct:: ramfs_fh_t
    :members:
