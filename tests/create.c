#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ramfs/ramfs.h"


int main(int argc, char *argv[])
{
    ramfs_fs_t *fs;
    ramfs_entry_t *file;

    fs = ramfs_init();
    assert(fs != NULL);

    file = ramfs_create(fs, "test", 0);
    assert(file != NULL);

    ramfs_deinit(fs);
    fs = NULL;

    exit(EXIT_SUCCESS);
    return 0;
}