/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ramfs/ramfs.h"
#include "ramfs/vfs.h"

#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs.h"

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <errno.h>

static const char* TAG = "ramfs";

#ifndef CONFIG_RAMFS_MAX_PARTITIONS
# define CONFIG_RAMFS_MAX_PARTITIONS 4
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

// Cleanup functions (saves using goto)
static void cleanup_malloc_char(char** p) {
	free(*p);
}


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

static esp_err_t ramfs_get_index_from_path(int *index, const char* base_path)
{
    int i;

    for (i = 0; i < CONFIG_RAMFS_MAX_PARTITIONS; i++) {
        if (s_ramfs_vfs[i] != NULL) {
			if (0 == strcmp(base_path, s_ramfs_vfs[i]->base_path)) {
	            *index = i;
	            return ESP_OK;
			}
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
    ESP_LOGV(TAG, "%s: path='%s', flags=%x, mode=%x", __func__, path, flags, mode);

    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    int fd;
    for (fd = 0; fd < vfs->fh_len; fd++) {
        if (vfs->fh[fd] == NULL) {
            break;
        }
    }
    if (fd >= vfs->fh_len) {
        ESP_LOGE(TAG, "%s: no free file descriptors", __func__);
		errno = ENFILE;
        return -1;
    }

	int cache_errno = errno;
    ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
	errno = cache_errno;

    if (NULL == entry) {
		if (flags & (O_CREAT | O_TRUNC)) {
	    	ESP_LOGD(TAG, "%s: Create new file: path='%s%s', flags=%x, mode=%x", __func__, vfs->base_path, path, flags, mode);
	        entry = ramfs_create(vfs->fs, path, flags);
			if (NULL == entry) {
	    		ESP_LOGW(TAG, "Failed to create new file at '%s%s': %s", vfs->base_path, path, strerror(errno));
				return -1;
			}
		} else {
			ESP_LOGW(TAG, "No file at '%s%s': %s", vfs->base_path, path, strerror(errno));
			return -1;
		}
    }
	assert(entry != NULL);

    ESP_LOGV(TAG, "%s: found entry for '%s', entry=%p", __func__, path, entry);


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
    if (rst.type == RAMFS_ENTRY_TYPE_DIR) {
        st->st_mode |= S_IFDIR;
    } else if (rst.type == RAMFS_ENTRY_TYPE_FILE) {
        st->st_mode |= S_IFREG;
    }
    return 0;
}

#if defined(CONFIG_RAMFS_VFS_SUPPORT_DIR)
static int ramfs_vfs_stat(void *ctx, const char *path, struct stat *st)
{
    ESP_LOGV(TAG, "%s: path='%s'", __func__, path);

    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;
    ramfs_stat_t rst;

	// Remove trailing slash if necessary
	// TODO: Move this into ramfs proper
	__attribute__((cleanup(cleanup_malloc_char)))
	char* temp = NULL;
	if (path[strlen(path)-1] == '/') {
		temp = strdup(path);
		if (NULL == temp) {
    		ESP_LOGV(TAG, "%s: RETURN=%d, errno=%d", __func__, -1, errno);
			return -1;
		}
		temp[strlen(path)-1] = '\0';
	}
	const char* norm_path = temp ? temp : path;	// Normalised path

	// If the trailing slash was removed, then it must be a directory; error otherwise
	bool trailing_slash_was_removed = (path[strlen(path)-1] == '/');


    ESP_LOGV(TAG, "calling %s(vfs->fs=%p, norm_path='%s'", "ramfs_get_entry", vfs->fs, norm_path);
    const ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, norm_path);
    if (entry == NULL) {
    	ESP_LOGV(TAG, "%s: RETURN=%d, errno=%d", __func__, -1, errno);
        return -1;
    }

    ramfs_stat(vfs->fs, entry, &rst);
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IRWXG | S_IRWXG | S_IRWXO;
    st->st_size = rst.size;
    if (rst.type == RAMFS_ENTRY_TYPE_DIR) {
        st->st_mode |= S_IFDIR;
    } else if (rst.type == RAMFS_ENTRY_TYPE_FILE) {
        st->st_mode |= S_IFREG;
    }

	// Trailing slash not allowed for files
	if (trailing_slash_was_removed) {
		if (false == S_ISDIR(st->st_mode)) {
			errno = ENOTDIR;
			return -1;
		}
	}

    ESP_LOGV(TAG, "%s: RETURN=%d", __func__, 0);
    return 0;
}

static int ramfs_vfs_unlink(void *ctx, const char *path)
{
    ESP_LOGV(TAG, "%s: path='%s'", __func__, path);

    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
        return -1;
    }

    return ramfs_unlink(entry);
}

static int ramfs_vfs_rename(void *ctx, const char *src, const char *dst)
{
    ESP_LOGV(TAG, "%s: src='%s' -> dst='%s'", __func__, src, dst);

    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    return ramfs_rename(vfs->fs, src, dst);
}

static DIR *ramfs_vfs_opendir(void *ctx, const char *path)
{
    ESP_LOGV(TAG, "%s: path='%s'", __func__, path);

    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;
    ramfs_vfs_dh_t *dh;

	// Remove trailing slash if necessary
	// TODO: Move this into ramfs proper
	__attribute__((cleanup(cleanup_malloc_char)))
	char* temp = NULL;
	if (path[strlen(path)-1] == '/') {
		temp = strdup(path);
		if (NULL == temp) {
			return NULL;
		}
		temp[strlen(path)-1] = '\0';
	}
	const char* norm_path = temp ? temp : path;	// Normalised path

    const ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, norm_path);
	if (NULL == entry) {
		return NULL;
	}

    ramfs_dh_t* ramfs_dh = ramfs_opendir(vfs->fs, entry);
	ESP_RETURN_ON_FALSE(
		ramfs_dh != NULL,
		NULL,
		TAG, "failed to open directory entry=%p, path='%s': %s", entry, norm_path, strerror(errno)
	);

    dh = calloc(1, sizeof(*dh));
    if (!dh) {
        ramfs_closedir(ramfs_dh);
        return NULL;
    }

    dh->dh = ramfs_dh;

    ESP_LOGV(TAG, "%s: RETURN=%p", __func__, dh);
    return (DIR *) dh;
}

static int ramfs_vfs_readdir_r(void *ctx, DIR *pdir, struct dirent *entry,
        struct dirent **out_ent);
static struct dirent *ramfs_vfs_readdir(void *ctx, DIR *pdir)
{
//    ESP_LOGV(TAG, "%s: pdir=%p", __func__, pdir);	// Redundant, readdir_r prints the same info

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
    ESP_LOGV(TAG, "%s: pdir=%p, ent=%p", __func__, pdir, ent);

    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;

    const ramfs_entry_t *entry = ramfs_readdir(dh->dh);
    ESP_LOGV(TAG, "%s: entry=%p", __func__, entry);
    if (entry == NULL) {
        *out_ent = NULL;
        return 0;
    }

    ent->d_ino = ramfs_telldir(dh->dh);
    char *name = ramfs_get_name(entry);
    strlcpy(ent->d_name, name, sizeof(ent->d_name));
    free(name);
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
    ESP_LOGV(TAG, "%s: pdir=%p", __func__, pdir);

    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;

    return ramfs_telldir(dh->dh);
}

static void ramfs_vfs_seekdir(void *ctx, DIR *pdir, long offset)
{
    ESP_LOGV(TAG, "%s: pdir=%p, offset=%ld", __func__, pdir, offset);

    ramfs_vfs_dh_t *dh = (ramfs_vfs_dh_t *) pdir;

    ramfs_seekdir(dh->dh, offset);
}

__attribute__((nonnull))
static int ramfs_vfs_mkdir(void *ctx, const char *path, mode_t mode)
{
    ESP_LOGV(TAG, "%s: path='%s', mode=0x%x", __func__, path, (unsigned int)mode);

    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

	// Remove trailing slash if necessary
	// TODO: Move this into ramfs proper
	__attribute__((cleanup(cleanup_malloc_char)))
	char* temp = NULL;
	if (path[strlen(path)-1] == '/') {
		temp = strdup(path);
		if (NULL == temp) {
			return -1;
		}
		temp[strlen(path)-1] = '\0';
	}
	const char* norm_path = temp ? temp : path;	// Normalised path
	
    ESP_LOGV(TAG, "calling %s(vfs->fs=%p, norm_path='%s'", "ramfs_mkdir", vfs->fs, norm_path);
	if (NULL == ramfs_mkdir(vfs->fs, norm_path)) {
		ESP_LOGW(TAG, "cannot make directory '%s%s': %s", vfs->base_path, norm_path, strerror(errno));
		return -1;
	}

	return 0;
}

__attribute__((nonnull))
static int ramfs_vfs_rmdir(void *ctx, const char *path)
{
    ESP_LOGV(TAG, "%s: path=%s", __func__, path);

    ramfs_vfs_t *vfs = (ramfs_vfs_t *) ctx;

    ramfs_entry_t *entry = ramfs_get_entry(vfs->fs, path);
    if (entry == NULL) {
    	ESP_LOGV(TAG, "%s: failed (errno=%d)", __func__, errno);
        return -1;
    }

    int ret = ramfs_rmdir(entry);
	if (ret != 0) {
		ESP_LOGV(TAG, "%s: failed (errno=%d)", __func__, errno);
	}
	return ret;
}

static int ramfs_vfs_closedir(void *ctx, DIR *pdir)
{
    ESP_LOGV(TAG, "%s: pdir=%p", __func__, pdir);

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
    ESP_LOGV(TAG, "%s: path=\"%s\", fs=%p, max_files=%d", __func__, conf->base_path, conf->fs, conf->max_files);

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
    strlcpy(vfs->base_path, conf->base_path, sizeof(vfs->base_path));
    vfs->fh_len = conf->max_files;

	// Check that the whole path was copied
	if (strcmp(vfs->base_path, conf->base_path) != 0) {
		ESP_LOGE(TAG, "base path '%s' is too long!", conf->base_path);	// If not, check the strlcpy() length parameter for bugs
		free(vfs);
		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGD(TAG, "register ramfs_vs, index=%d, base_path='%s'", index, vfs->base_path);

    esp_err_t err = esp_vfs_register(vfs->base_path, &funcs, vfs);
    if (err != ESP_OK) {
        free(vfs);
        return err;
    }

    s_ramfs_vfs[index] = vfs;
    
	return ESP_OK;
}

esp_err_t ramfs_vfs_unregister(const ramfs_vfs_conf_t *conf)
{
	int i;
	ESP_RETURN_ON_ERROR(
		ramfs_get_index_from_path(&i, conf->base_path),
		TAG, "no ramfs found for '%s'", conf->base_path
	);

	s_ramfs_vfs[i] = NULL;

	return esp_vfs_unregister(conf->base_path);
}

