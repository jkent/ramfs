/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ramfs/ramfs.h"
#include "ramfs/vfs.h"

#include "esp_err.h"
#include "esp_vfs.h"

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>


#ifndef CONFIG_RAMFS_MAX_PARTITIONS
# define CONFIG_RAMFS_MAX_PARTITIONS 1
#endif

#if defined(CONFIG_RAMFS_VFS_SUPPORT_DIR)
typedef struct {
    DIR dir;
    ramfs_dh_t *dh;
    struct dirent dirent;
} ramfs_vfs_dh_t;
#endif

typedef struct {
    ramfs_fs_t *fs;
    char base_path[ESP_VFS_PATH_MAX + 1];
    size_t fh_len;
    ramfs_fh_t *fh[];
} ramfs_vfs_t;

static ramfs_vfs_t *s_ramfs_vfs[CONFIG_RAMFS_MAX_PARTITIONS];

static esp_err_t ramfs_get_empty(int *index)
{
    int i;

    for (i = 0; i < CONFIG_RAMFS_MAX_PARTITIONS; i++) {
        if (s_ramfs_vfs[i] == NULL) {
            *index = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static ssize_t ramfs_vfs_write(void *ctx, int fd, const void *data, size_t size)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    if (fd < 0 || fd >= vfs->fh_len || vfs->fh[fd] == NULL) {
        return -1;
    }

    return ramfs_write(vfs->fh[fd], data, size);
}

static off_t ramfs_vfs_lseek(void *ctx, int fd, off_t offset, int mode)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    if (fd < 0 || fd >= vfs->fh_len || vfs->fh[fd] == NULL) {
        return -1;
    }

    return ramfs_seek(vfs->fh[fd], offset, mode);
}

static ssize_t ramfs_vfs_read(void *ctx, int fd, void *data, size_t size)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    if (fd < 0 || fd >= vfs->fh_len || vfs->fh[fd] == NULL) {
        return -1;
    }

    return ramfs_read(vfs->fh[fd], data, size);
}

static int ramfs_vfs_open(void *ctx, const char *path, int flags, int mode)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    int fd;
    for (fd = 0; fd < vfs->fh_len; fd++) {
        if (vfs->fh[fd] == NULL) {
            break;
        }
    }
    if (fd >= vfs->fh_len) {
        return -1;
    }

    ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);

    if (entry == NULL && flags & (O_CREAT | O_TRUNC)) {
        entry = ramfs_create(vfs->fs, path, flags);
        if (entry == NULL) {
            return -1;
        }
    }

    vfs->fh[fd] = ramfs_open(vfs->fs, entry, flags);
    return fd;
}

static int ramfs_vfs_close(void *ctx, int fd)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    if (fd < 0 || fd >= vfs->fh_len || vfs->fh[fd] == NULL) {
        return -1;
    }

    ramfs_close(vfs->fh[fd]);
    vfs->fh[fd] = NULL;
    return 0;
}

static int ramfs_vfs_fstat(void *ctx, int fd, struct stat *st)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;
    ramfs_stat_t rst;

    if (fd < 0 || fd >= vfs->fh_len || vfs->fh[fd] == NULL) {
        return -1;
    }

    ramfs_stat(vfs->fs, vfs->fh[fd]->entry, &rst);
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IRWXG | S_IRWXG | S_IRWXO;
    st->st_size = rst.size;
    if (st->st_mode == RAMFS_ENTRY_TYPE_DIR) {
        st->st_mode |= S_IFDIR;
    } else if (st->st_mode == RAMFS_ENTRY_TYPE_FILE) {
        st->st_mode |= S_IFREG;
    }
    return 0;
}

#if defined(CONFIG_RAMFS_VFS_SUPPORT_DIR)
static int ramfs_vfs_stat(void *ctx, const char *path, struct stat *st)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;
    ramfs_stat_t rst;

    const ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
        return -1;
    }

    ramfs_stat(vfs->fs, entry, &rst);
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IRWXG | S_IRWXG | S_IRWXO;
    st->st_size = rst.size;
    if (st->st_mode == RAMFS_ENTRY_TYPE_DIR) {
        st->st_mode |= S_IFDIR;
    } else if (st->st_mode == RAMFS_ENTRY_TYPE_FILE) {
        st->st_mode |= S_IFREG;
    }
    return 0;
}

static int ramfs_vfs_unlink(void *ctx, const char *path)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
        return -1;
    }

    return ramfs_unlink(entry);
}

static int ramfs_vfs_rename(void *ctx, const char *src, const char *dst)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    return ramfs_rename(vfs->fs, src, dst);
}

static DIR *ramfs_vfs_opendir(void *ctx, const char *path)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;
    ramfs_vfs_dh_t *dh = malloc(sizeof(*dh));

    const ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
        return NULL;
    }

    dh->dh = ramfs_opendir(vfs->fs, entry);
    return (DIR *) dh;
}

static int ramfs_vfs_readdir_r(void *ctx, DIR *pdir, struct dirent *entry,
        struct dirent **out_ent);
static struct dirent *ramfs_vfs_readdir(void *ctx, DIR *pdir)
{
    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;
    struct dirent *out_ent;

    int err = ramfs_vfs_readdir_r(ctx, pdir, &dh->dirent, &out_ent);
    if (err != 0) {
        return NULL;
    }

    return out_ent;
}

static int ramfs_vfs_readdir_r(void *ctx, DIR *pdir, struct dirent *ent,
        struct dirent **out_ent)
{
    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;

    const ramfs_entry_t *entry = ramfs_readdir(dh->dh);
    if (entry == NULL) {
        *out_ent = NULL;
        return 0;
    }

    ent->d_ino = ramfs_telldir(dh->dh);
    const char *name = ramfs_get_name(entry);
    strlcpy(ent->d_name, name, sizeof(ent->d_name));
    ent->d_type = DT_UNKNOWN;
    if (ramfs_is_dir(entry)) {
        ent->d_type = DT_DIR;
    } else if (ramfs_is_file(entry)) {
        ent->d_type = DT_REG;
    }
    *out_ent = ent;
    return 0;
}

static long ramfs_vfs_telldir(void *ctx, DIR *pdir)
{
    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;

    return ramfs_telldir(dh->dh);
}

static void ramfs_vfs_seekdir(void *ctx, DIR *pdir, long offset)
{
    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;

    ramfs_seekdir(dh->dh, offset);
}

static int ramfs_vfs_mkdir(void *ctx, const char *path, mode_t mode)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    return ramfs_mkdir(vfs->fs, path) != NULL;
}

static int ramfs_vfs_rmdir(void *ctx, const char *path)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
        return -1;
    }

    return ramfs_rmdir(entry);
}

static int ramfs_vfs_closedir(void *ctx, DIR *pdir)
{
    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;

    ramfs_closedir(dh->dh);
    dh->dh = NULL;
    return 0;
}

static int ramfs_vfs_access(void *ctx, const char *path, int amode)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    const ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
        return -1;
    }

    return 0;
}

static int ramfs_vfs_truncate(void *ctx, const char *path, off_t length)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
        return -1;
    }

    return ramfs_truncate(vfs->fs, entry, length);
}

static int ramfs_vfs_ftruncate(void *ctx, int fd, off_t length)
{
    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    if (fd < 0 || fd >= vfs->fh_len || vfs->fh[fd] == NULL) {
        return -1;
    }

    ramfs_entry_t *entry = vfs->fh[fd]->entry;

    return ramfs_truncate(vfs->fs, entry, length);
}
#endif

esp_err_t ramfs_vfs_register(const ramfs_vfs_conf_t *conf)
{
    assert(conf != NULL);
    assert(conf->fs != NULL);
    assert(conf->base_path != NULL);

    const esp_vfs_t funcs = {
        .flags = ESP_VFS_FLAG_CONTEXT_PTR,
        .write_p = &ramfs_vfs_write,
        .lseek_p = &ramfs_vfs_lseek,
        .read_p = &ramfs_vfs_read,
        .open_p = &ramfs_vfs_open,
        .close_p = &ramfs_vfs_close,
        .fstat_p = &ramfs_vfs_fstat,
#ifdef CONFIG_RAMFS_VFS_SUPPORT_DIR
        .stat_p = &ramfs_vfs_stat,
        .unlink_p = &ramfs_vfs_unlink,
        .rename_p = &ramfs_vfs_rename,
        .opendir_p = &ramfs_vfs_opendir,
        .readdir_p = &ramfs_vfs_readdir,
        .readdir_r_p = &ramfs_vfs_readdir_r,
        .telldir_p = &ramfs_vfs_telldir,
        .seekdir_p = &ramfs_vfs_seekdir,
        .closedir_p = &ramfs_vfs_closedir,
        .mkdir_p = &ramfs_vfs_mkdir,
        .rmdir_p = &ramfs_vfs_rmdir,
        .access_p = &ramfs_vfs_access,
        .truncate_p = &ramfs_vfs_truncate,
        .ftruncate_p = &ramfs_vfs_ftruncate,
#endif
    };

    int index;
    if (ramfs_get_empty(&index) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    ramfs_vfs_t *vfs = calloc(1, sizeof(ramfs_vfs_t) +
            (sizeof(ramfs_fh_t) * conf->max_files));
    if (vfs == NULL) {
        return ESP_ERR_NO_MEM;
    }

    vfs->fs = conf->fs;
    strlcpy(vfs->base_path, conf->base_path, sizeof(*vfs->base_path));
    vfs->fh_len = conf->max_files;

    esp_err_t err = esp_vfs_register(vfs->base_path, &funcs, vfs);
    if (err != ESP_OK) {
        free(vfs);
        return err;
    }

    s_ramfs_vfs[index] = vfs;
    return ESP_OK;
}
