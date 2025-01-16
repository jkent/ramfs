#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ramfs/ramfs.h"


int main(int argc, char *argv[])
{
    ramfs_fs_t *fs;
    ramfs_entry_t *dir;

    fs = ramfs_init();
    assert(fs != NULL);

    dir = ramfs_mkdir(fs, "test");
    assert(dir != NULL);

    ramfs_deinit(fs);
    fs = NULL;

    exit(EXIT_SUCCESS);
    return 0;
}