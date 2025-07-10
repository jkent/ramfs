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
    ramfs_entry_t *file;
    ramfs_fh_t *fh;
    char buf[12];

    fs = ramfs_init();
    assert(fs != NULL);

    file = ramfs_create(fs, "test", 0);
    assert(file != NULL);

    fh = ramfs_open(fs, file, O_WRONLY);
    assert(fh != NULL);

    assert(ramfs_write(fh, "Hello World!", 12) == 12);

    ramfs_close(fh);

    fh = ramfs_open(fs, file, O_RDONLY);
    assert(fh != NULL);

    assert(ramfs_read(fh, buf, 12) == 12);

    assert(memcmp(buf, "Hello World!", 12) == 0);

    ramfs_close(fh);

    ramfs_deinit(fs);
    fs = NULL;

    exit(EXIT_SUCCESS);
    return 0;
}