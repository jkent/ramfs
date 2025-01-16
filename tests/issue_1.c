#include <assert.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ramfs/ramfs.h"


int main(int argc, char *argv[])
{
    ramfs_fs_t *fs;
    ramfs_entry_t *dir, *file;
    ramfs_fh_t *fh;
    char buf[25];

    fs = ramfs_init();
    assert(fs != NULL);

    dir = ramfs_mkdir(fs, "dir");
    assert(dir != NULL);

    file = ramfs_create(fs, "dir/test_file.txt", 0);
    assert(file != NULL);

    fh = ramfs_open(fs, file, O_WRONLY);
    assert(fh != NULL);

    assert(ramfs_write(fh, "This is dir/test_file.txt", 25) == 25);

    ramfs_close(fh);

    assert(ramfs_rename(fs, "dir/test_file.txt", "dir/test_file_new.txt") == 0);

    file = ramfs_get_entry(fs, "dir/test_file_new.txt");
    assert(file != NULL);

    fh = ramfs_open(fs, file, O_RDONLY);
    assert(fh != NULL);

    assert(ramfs_read(fh, buf, 25) == 25);

    assert(memcmp(buf, "This is dir/test_file.txt", 25) == 0);

    ramfs_close(fh);

    file = ramfs_get_entry(fs, "dir/test_file.txt");
    assert(file == NULL);

    file = ramfs_get_entry(fs, "dir/test_file_new.txt");
    assert(file != NULL);
    assert(ramfs_unlink(file) == 0);

    file = ramfs_get_entry(fs, "dir/test_file_new.txt");
    assert(file == NULL);

    ramfs_deinit(fs);
    fs = NULL;

    exit(EXIT_SUCCESS);
    return 0;
}