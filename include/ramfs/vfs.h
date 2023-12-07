/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "ramfs.h"

#include "esp_err.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief       Configuration structure for the \a ramfs_vfs_register function
 */
typedef struct ramfs_vfs_conf_t {
    const char *base_path; /**< vfs path to mount the filesystem */
    ramfs_fs_t *fs; /**< the ramfs instance */
    size_t max_files; /**< maximum open files */
} ramfs_vfs_conf_t;

/**
 * \brief      Mount an ramfs fs handle under a vfs path
 * \param[in]  conf vfs configuration
 * \return     ESP_OK if successful, ESP_ERR_NO_MEM if too many VFSes are
 *             registered
 */
esp_err_t ramfs_vfs_register(const ramfs_vfs_conf_t *conf);

#ifdef __cplusplus
} /* extern "C" */
#endif
