#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ramfs/ramfs.h"


int main(int argc, char *argv[])
{
    ramfs_fs_t *fs;

    fs = ramfs_init();
    assert(fs != NULL);

    exit(EXIT_SUCCESS);
    return 0;
}