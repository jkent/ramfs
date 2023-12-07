# About RamFS

RamFS is a memory based filesystem designed for embedded use. It can be easily
used with a CMake project &mdash; including
[ESP-IDF](https://github.com/espressif/esp-idf).

# Getting started with ESP-IDF

To use this component with ESP-IDF, within your projects directory run

    idf.py add-dependency jkent/ramfs

## Usage

Two interfaces are available: the [bare API](#bare-api) or when using IDF
there is the [VFS interface](#vfs-interface) which builds on top of the bare
API. You should use the VFS interface in IDF projects, as it uses the portable
and familiar `posix` and `stdio` C functions with it. There is nothing
preventing you from mix and matching both at the same time, however.

### Shared initialization

Initialization is as simple as calling `ramfs_init` function and checking its
return variable:

```C
ramfs_fs_t *fs = ramfs_init();
assert(fs != NULL);
```

When done, and all file handles are closed, you can call `ramfs_deinit`:

```C
ramfs_deinit(fs);
```

### VFS interface

The VFS interface adds another step to the initialization: you define a
`ramfs_vfs_conf_t` structure:

  * **base_path** - path to mount the ramfs
  * **fs** - a `ramfs_fs_t` instance
  * **max_files** - max number of files that can be open at a time

```C
ramfs_vfs_conf_t ramfs_vfs_conf = {
    .base_path = "/ramfs",
    .fs = fs,
    .max_files = 5,
};
ramfs_vfs_register(&ramfs_vfs_conf);
```

### Bare API

#### Filesystem functions:

  * ramfs_fs_t *[ramfs_init](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_init)(void)
  * void [ramfs_deinit](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_deinit)(ramfs_fs_t *fs)

#### Object functions:

  * const ramfs_entry_t *[ramfs_get_parent](https://ramfs.readthedocs.io/en/latest/apo-reference/bare.html#c.ramfs_get_parent)(ramfs_fs_t *fs, const char *path)
  * const ramfs_entry_t *[ramfs_get_entry](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_get_entry)(ramfs_fs_t *fs, const char *path)
  * const char *[ramfs_get_name](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_get_name)(const ramfs_entry_t *entry)
  * const char *[ramfs_get_path](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_get_path)(const ramfs_entry_t *entry)
  * int [ramfs_is_dir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_is_dir)(const ramfs_entry_t *entry)
  * int [ramfs_is_file](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_is_file)(const ramfs_entry_t *entry)
  * void [ramfs_stat](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_stat)(const ramfs_fs_t *fs, const ramfs_entry_t *entry, ramfs_stat_t *st)
  * void [ramfs_create](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_create)(ramfs_fs_t *fs, const char *path, int flags)
  * void [ramfs_truncate](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_truncate)(ramfs_fs_t *fs, const ramfs_entry_t *entry, size_T size)
  * ramfs_fh_t *[ramfs_open](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_open)(ramfs_fs_t *fs, const ramfs_entry_t *entry, unsigned int flags)
  * void [ramfs_close](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_close)(ramfs_fh_t *fh)
  * size_t [ramfs_read](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_read)(ramfs_fh_t *fh, void *buf, size_t len)
  * size_t [ramfs_write](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_write)(ramfs_fh_t *fh, void *buf, size_t len)
  * ssize_t [ramfs_seek](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_seek)(ramfs_fh_t *fh, long offset, int mode)
  * size_t [ramfs_tell](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_tell)(ramfs_fh_t *fh)
  * size_t [ramfs_access](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_access)(ramfs_fh_t *fh, void **buf)
  * int [ramfs_unlink](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.unink)(ramfs_entry_t *entry)
  * int [ramfs_rename](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.rename)(ramfs_fs_t *fs, const char *src, const char *dst)

#### Directory Functions:

  * ramfs_dh_t *[ramfs_opendir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_opendir)(ramfs_fs_t *fs, const ramfs_entry_t *entry)
  * void [ramfs_closedir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_closedir)(ramfs_dh_t *dh)
  * const ramfs_entry_t *[ramfs_readdir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_readdir)(ramfs_dh_t *dh)
  * void [ramfs_seekdir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_seekdir)(ramfs_dh_t *dh, size_t loc)
  * size_t [ramfs_telldir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_telldir)(ramfs_dh_t *dh)
  * ramfs_entry_t *[ramfs_mkdir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_mkdir)(ramfs_fs_t *fs, const char *name)
  * int [ramfs_rmdir](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_rmdir)(ramfs_entry_t *entry)
  * void [ramfs_rmtree](https://ramfs.readthedocs.io/en/latest/api-reference/bare.html#c.ramfs_rmtree)(ramfs_entry_t *entry)
