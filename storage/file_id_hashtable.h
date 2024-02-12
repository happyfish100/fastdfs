/*
 * Copyright (c) 2020 YuQing <384681@qq.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _FILE_ID_HASHTABLE_H
#define _FILE_ID_HASHTABLE_H

#include "storage_types.h"

#ifdef __cplusplus
extern "C" {
#endif

    int file_id_hashtable_init();

    void file_id_hashtable_destroy();

    int file_id_hashtable_add(const string_t *file_id);

#ifdef __cplusplus
}
#endif

#endif
