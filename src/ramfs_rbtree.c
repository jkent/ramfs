/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rbtree.h"


#define RAMFS_PRIVATE_STRUCTS
typedef struct ramfs_fs_t ramfs_fs_t;
typedef struct ramfs_dir_t ramfs_dir_t;

/* format structures */
typedef struct ramfs_entry_t {
    ramfs_rbnode_t rbnode;
    ramfs_dir_t *parent;
    int type;
} ramfs_entry_t;

typedef struct ramfs_dir_t {
    ramfs_entry_t entry;
    ramfs_rbtree_t rbtree;
} ramfs_dir_t;

typedef struct ramfs_file_t {
    ramfs_entry_t entry;
    unsigned char *data;
    size_t size;
} ramfs_file_t;

/* user handles */
typedef struct ramfs_fs_t {
    ramfs_dir_t root;
} ramfs_fs_t;

typedef struct ramfs_dh_t {
    ramfs_fs_t *fs;
    ramfs_dir_t *dir;
    ramfs_entry_t *entry;
    size_t loc;
} ramfs_dh_t;

typedef struct ramfs_fh_t {
    ramfs_fs_t *fs;
    ramfs_file_t *file;
    int flags;
    size_t pos;
} ramfs_fh_t;

#include "ramfs/ramfs.h"


static int ramfs_cmp(const void *left, const void *right)
{
    if (left == NULL) {
        if (right == NULL) {
            return 0;
        }
        return -1;
    } else if (right == NULL) {
        return 1;
    }

    return strcmp((const char *) left, (const char *) right);
}

ramfs_fs_t *ramfs_init(void)
{
    ramfs_fs_t *fs = calloc(1, sizeof(*fs));

    if (fs == NULL) {
        return NULL;
    }

    ramfs_rbtree_init(&fs->root.rbtree, ramfs_cmp);
    return fs;
}

void ramfs_deinit(ramfs_fs_t *fs)
{
    assert(fs != NULL);

    ramfs_rmtree(&fs->root.entry);
    free(fs);
}

ramfs_entry_t *ramfs_get_parent(ramfs_fs_t *fs, const char *path)
{
    ramfs_dir_t *dir;

    assert(fs != NULL);
    assert(path != NULL);

    dir = &fs->root;

    while (*path == '/') {
        path++;
    }

    const char *end;
    while ((end = strchr(path, '/')) != NULL) {
        char *key = strndup(path, end - path);
        if (key == NULL) {
            errno = ENOMEM;
            return NULL;
        }
        dir = (ramfs_dir_t *) ramfs_rbtree_search(&dir->rbtree, key);
        free(key);
        path = end + 1;
        while (*path == '/') {
            path++;
        }
    }

    return &dir->entry;
}

ramfs_entry_t *ramfs_get_entry(ramfs_fs_t *fs, const char *path)
{
    assert(fs != NULL);
    assert(path != NULL);

    while (*path == '/') {
        path++;
    }

    ramfs_dir_t *parent = (ramfs_dir_t *) ramfs_get_parent(fs, path);
    if (parent == NULL) {
        return NULL;
    }

    size_t len = strlen(path);
    const char *key = path + len;
    while (key > path && *(key - 1) != '/') {
        key--;
    }

    if (len - (key - path) == 0) {
        errno = EINVAL;
        return NULL;
    }

    return (ramfs_entry_t *) ramfs_rbtree_search(&parent->rbtree, key);
}

char *ramfs_get_name(const ramfs_entry_t *entry)
{
    assert(entry != NULL);

    return strdup(entry->rbnode.key);
}

char *ramfs_get_path(const ramfs_entry_t *entry)
{
    assert(entry != NULL);

    size_t len = 0;
    const ramfs_entry_t *node = entry;

    while (node != NULL) {
        len += strlen((char *) node->rbnode.key) + 1;
        node = &entry->parent->entry;
    }

    char *path = malloc(len + 1);
    path[len] = '\0';

    node = entry;
    while (node != NULL) {
        int name_len = strlen((char *) node->rbnode.key);
        len -= name_len;
        memcpy(path + len, (char *) node->rbnode.key, name_len);
        path[--len] = '/';
        node = &entry->parent->entry;
    }

    return path;
}

int ramfs_is_dir(const ramfs_entry_t *entry)
{
    assert(entry != NULL);

    return entry->type == RAMFS_ENTRY_TYPE_DIR;
}

int ramfs_is_file(const ramfs_entry_t *entry)
{
    assert(entry != NULL);

    return entry->type == RAMFS_ENTRY_TYPE_FILE;
}

void ramfs_stat(ramfs_fs_t *fs, const ramfs_entry_t *entry, ramfs_stat_t *st)
{
    assert(fs != NULL);
    assert(entry != NULL);
    assert(st != NULL);

    memset(st, 0, sizeof(*st));
    st->type = entry->type;
    if (entry->type == RAMFS_ENTRY_TYPE_FILE) {
        ramfs_file_t *file = (ramfs_file_t *) entry;
        st->size = file->size;
    }
}

ramfs_entry_t *ramfs_create(ramfs_fs_t *fs, const char *path, int flags)
{
    ramfs_file_t *file;

    assert(fs != NULL);
    assert(path != NULL);

    while (*path == '/') {
        path++;
    }

    ramfs_dir_t *parent = (ramfs_dir_t *) ramfs_get_parent(fs, path);
    if (parent == NULL) {
        return NULL;
    }

    size_t len = strlen(path);
    const char *name = path + len;
    while (name > path && *(name - 1) != '/') {
        name--;
    }
    if (len - (path - name) == 0) {
        errno = EINVAL;
        return NULL;
    }

    file = calloc(1, sizeof(*file));
    if (file == NULL) {
        return NULL;
    }

    file->entry.rbnode.key = strdup(name);
    if (file->entry.rbnode.key == NULL) {
        free(file);
        return NULL;
    }
    file->entry.parent = parent;
    file->entry.type = RAMFS_ENTRY_TYPE_FILE;
    if (ramfs_rbtree_insert(&parent->rbtree, &file->entry.rbnode) == NULL) {
        free((void *) file->entry.rbnode.key);
        free(file);
        errno = EEXIST;
        return NULL;
    }

    return &file->entry;
}

int ramfs_truncate(ramfs_fs_t *fs, ramfs_entry_t *entry, size_t size)
{
    assert(fs != NULL);
    assert(entry != NULL);

    if (entry->type != RAMFS_ENTRY_TYPE_FILE) {
        return -1;
    }

    ramfs_file_t *file = (ramfs_file_t *) entry;

    unsigned char *new_data = realloc(file->data, size);
    if (new_data == NULL) {
        return -1;
    }
    file->data = new_data;

    if (size > file->size) {
        memset(file->data + file->size, 0, size - file->size);
        file->size = size;
    }

    return 0;
}

ramfs_fh_t *ramfs_open(ramfs_fs_t *fs, const ramfs_entry_t *entry,
        unsigned int flags)
{
    assert(fs != NULL);
    assert(entry != NULL);

    if (entry->type != RAMFS_ENTRY_TYPE_FILE) {
        return NULL;
    }

    ramfs_file_t *file = (ramfs_file_t *) entry;

    if (flags & O_TRUNC) {
        free(file->data);
        file->data = NULL;
        file->size = 0;
    }

    ramfs_fh_t *fh = calloc(1, sizeof(*fh));
    if (fh == NULL) {
        return NULL;
    }

    if (flags & O_APPEND) {
        fh->pos = file->size;
    }

    fh->file = file;
    fh->flags = flags;
    return fh;
}


void ramfs_close(ramfs_fh_t *fh)
{
    assert(fh != NULL);

    free(fh);
}

ssize_t ramfs_read(ramfs_fh_t *fh, char *buf, size_t len)
{
    assert(fh != NULL);
    assert(buf != NULL);

    if (fh->pos >= fh->file->size) {
        return 0;
    }

    if (len > fh->file->size - fh->pos) {
        len = fh->file->size - fh->pos;
    }

    memcpy(buf, fh->file->data + fh->pos, len);
    fh->pos += len;
    return len;
}

ssize_t ramfs_write(ramfs_fh_t *fh, const char *buf, size_t len)
{
    assert(fh != NULL);
    assert(buf != NULL);

    if (!(fh->flags & O_WRONLY || fh->flags & O_RDWR)) {
        errno = EBADF;
        return -1;
    }

    if (fh->pos + len > fh->file->size) {
        size_t new_size = fh->pos + len;
        unsigned char *p = realloc(fh->file->data, new_size);
        if (new_size != 0 && p == NULL) {
            return -1;
        }
        fh->file->data = p;
        if (fh->pos > fh->file->size) {
            memset(fh->file->data + fh->file->size, 0,
                    fh->pos - fh->file->size);
        }
        fh->file->size = fh->pos + len;
    }

    memcpy(fh->file->data + fh->pos, buf, len);
    fh->pos += len;
    return len;
}

ssize_t ramfs_seek(ramfs_fh_t *fh, off_t offset, int whence)
{
    assert(fh != NULL);

    ssize_t pos = fh->pos;

    if (whence == SEEK_CUR) {
        pos += offset;
    } else if (whence == SEEK_SET) {
        pos = offset;
    } else if (whence == SEEK_END) {
        pos = fh->file->size + offset;
    }

    if (pos < 0) {
        pos = 0;
    }

    fh->pos = pos;
    return pos;
}

size_t ramfs_tell(const ramfs_fh_t *fh)
{
    assert(fh != NULL);

    return fh->pos;
}

size_t ramfs_access(const ramfs_fh_t *fh, const void **buf)
{
    assert(fh != NULL);

    *buf = fh->file->data;
    return fh->file->size;
}

int ramfs_unlink(ramfs_entry_t *entry)
{
    assert(entry != NULL);

    if (entry->type != RAMFS_ENTRY_TYPE_FILE) {
        errno = ENFILE;
        return -1;
    }

    ramfs_rbtree_delete_node(&entry->parent->rbtree, &entry->rbnode);
    free((void *) entry->rbnode.key);
    free(entry);
    return 0;
}

int ramfs_rename(ramfs_fs_t *fs, const char *src, const char *dst)
{
    assert(src != NULL);
    assert(dst != NULL);

    if (strcmp(src, dst) == 0) {
        return 0;
    }

    while (*src == '/') {
        src++;
    }
    ramfs_dir_t *src_parent = (ramfs_dir_t *) ramfs_get_parent(fs, src);
    if (src_parent == NULL) {
        errno = ENOENT;
        return -1;
    }

    while(*dst == '/') {
        dst++;
    }
    ramfs_dir_t *dst_parent = (ramfs_dir_t *) ramfs_get_parent(fs, dst);
    if (dst_parent == NULL) {
        errno = ENOENT;
        return -1;
    }

    size_t len = strlen(src);
    const char *name = src + len;
    while (name > src && *(name - 1) != '/') {
        name--;
    }
    ramfs_entry_t *src_entry =
            (ramfs_entry_t *) ramfs_rbtree_search(&src_parent->rbtree, name);
    if (src_entry == NULL) {
        errno = ENOENT;
        return -1;
    }

    len = strlen(dst);
    name = dst + len;
    while (name > dst && *(name - 1) != '/') {
        name--;
    }
    ramfs_entry_t *dst_entry =
            (ramfs_entry_t *) ramfs_rbtree_search(&dst_parent->rbtree, name);
    if (dst_entry != NULL) {
        errno = EEXIST;
        return -1;
    }

    name = strdup(name);
    if (name == NULL) {
        return -1;
    }

    ramfs_rbtree_delete_node(&src_parent->rbtree, &src_entry->rbnode);
    free((void *) src_entry->rbnode.key);
    src_entry->rbnode.key = (char *) name;
    ramfs_rbtree_insert(&dst_parent->rbtree, &src_entry->rbnode);
    return 0;
}

ramfs_dh_t *ramfs_opendir(ramfs_fs_t *fs, const ramfs_entry_t *entry)
{
    assert(fs != NULL);
    assert(entry != NULL);

    if (ramfs_is_file(entry)) {
        errno = ENOTDIR;
        return NULL;
    }

    ramfs_dh_t *dh = calloc(1, sizeof(*dh));
    dh->dir = (ramfs_dir_t *) entry;
    return dh;
}

void ramfs_closedir(ramfs_dh_t *dh)
{
    assert(dh != NULL);

    free(dh);
}

const ramfs_entry_t *ramfs_readdir(ramfs_dh_t *dh)
{
    assert(dh != NULL);

    if (dh->loc == 0) {
        dh->entry = (ramfs_entry_t *) ramfs_rbtree_first(&dh->dir->rbtree);
    } else if (dh->entry != NULL) {
        dh->entry = (ramfs_entry_t *) ramfs_rbtree_next(&dh->entry->rbnode);
    }

    if (dh->entry != NULL) {
        dh->loc++;
    }

    return dh->entry;
}

void ramfs_seekdir(ramfs_dh_t *dh, long loc)
{
    assert(dh != NULL);
    assert(loc >= 0);

    ramfs_rbtree_t *rbtree = &dh->entry->parent->rbtree;

    if (loc == 0) {
        dh->loc = 0;
    } else {
        while (loc < dh->loc && loc > 0) {
            dh->entry = (ramfs_entry_t *) ramfs_rbtree_previous(&dh->entry->rbnode);
            dh->loc--;
        }
        while (loc > dh->loc && dh->loc < rbtree->count) {
            dh->entry = (ramfs_entry_t *) ramfs_rbtree_next(&dh->entry->rbnode);
            dh->loc++;
        }
    }
}

long ramfs_telldir(ramfs_dh_t *dh)
{
    assert(dh != NULL);

    return dh->loc;
}

ramfs_entry_t *ramfs_mkdir(ramfs_fs_t *fs, const char *path)
{
    assert(fs != NULL);
    assert(path != NULL);

    while (*path == '/') {
        path++;
    }

    ramfs_dir_t *parent = (ramfs_dir_t *) ramfs_get_parent(fs, path);
    if (parent == NULL) {
        return NULL;
    }

    size_t len = strlen(path);
    const char *name = path + len;
    while (name > path && *(name - 1) != '/') {
        name--;
    }
    if (len - (path - name) == 0) {
        name--;
        errno = EINVAL;
        return NULL;
    }

    if (strlen(name) == 0 || strchr(name, '/')) {
        errno = EINVAL;
        return NULL;
    }

    ramfs_rbtree_t *rbtree = &((ramfs_dir_t *) parent)->rbtree;

    ramfs_dir_t *dir = calloc(1, sizeof(*dir));
    if (dir == NULL) {
        return NULL;
    }

    dir->entry.rbnode.key = strdup(name);
    if (dir->entry.rbnode.key == NULL) {
        free(dir);
        return NULL;
    }
    dir->entry.parent = (ramfs_dir_t *) parent;
    dir->entry.type = RAMFS_ENTRY_TYPE_DIR;
    ramfs_rbtree_init(&dir->rbtree, ramfs_cmp);
    return (ramfs_entry_t *) ramfs_rbtree_insert(rbtree, &dir->entry.rbnode);
}

int ramfs_rmdir(ramfs_entry_t *entry)
{
    assert(entry != NULL);

    if (!ramfs_is_dir(entry)) {
        errno = ENOTDIR;
        return -1;
    }

    ramfs_dir_t *dir = (ramfs_dir_t *) entry;
    ramfs_dir_t *parent = entry->parent;

    if (dir->rbtree.count > 0) {
        errno = ENOTEMPTY;
        return -1;
    }

    ramfs_rbtree_delete_node(&parent->rbtree, &dir->entry.rbnode);
    free((void *) dir->entry.rbnode.key);
    free(dir);
    return 0;
}

void ramfs_rmtree(ramfs_entry_t *entry)
{
    assert(entry != NULL);

    if (ramfs_is_file(entry)) {
        ramfs_unlink(entry);
        return;
    }

    ramfs_entry_t *child;
    ramfs_dir_t *dir = (ramfs_dir_t *) entry;
    ramfs_rbtree_t *rbtree = (ramfs_rbtree_t *) dir->rbtree.root;

    while ((child = (ramfs_entry_t *) ramfs_rbtree_last(rbtree)) != NULL) {
        if (ramfs_is_dir(child)) {
            ramfs_rmtree(child);
        } else {
            free(((ramfs_file_t *) child)->data);
        }
        ramfs_rbtree_delete_node(rbtree, &child->rbnode);
        free((void *) child->rbnode.key);
        free(child);
    }
    if (entry->parent != NULL) {
        free((void *) entry->rbnode.key);
        free(entry);
    }
}
