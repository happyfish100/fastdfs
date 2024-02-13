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

#include "fastcommon/pthread_func.h"
#include "fastcommon/logger.h"
#include "fastcommon/fc_atomic.h"
#include "fastcommon/fast_allocator.h"
#include "file_id_hashtable.h"

typedef struct file_id_info {
    string_t file_id;
    uint32_t hash_code;
    uint32_t expires;
    struct {
        struct file_id_info *htable;
        struct file_id_info *list;
    } nexts;
} FileIdInfo;

typedef struct {
    FileIdInfo **buckets;
    uint32_t capacity;
#if defined(DEBUG_FLAG)
    volatile uint32_t count;
#endif
} FileIdHashtable;

typedef struct {
    pthread_mutex_t *locks;
    int count;
} FileIdSharedLockArray;

typedef struct {
    struct {
        struct file_id_info *head;
        struct file_id_info *tail;
        pthread_mutex_t lock;
    } list;
    FileIdHashtable htable;
    FileIdSharedLockArray lock_array;
    struct fast_mblock_man allocator;  //element: FileIdInfo
    struct fast_allocator_context acontext; //for string allocator
} FileIdHTableContext;

static FileIdHTableContext file_id_ctx = {
    {NULL, NULL}, {NULL, 0}, {NULL, 0}
};

static int clear_expired_file_id_func(void *args);

int file_id_hashtable_init()
{
    const int obj_size = 0;
    int result;
    int bytes;
    struct fast_region_info regions[2];
    pthread_mutex_t *lock;
    pthread_mutex_t *end;
    ScheduleArray scheduleArray;
    ScheduleEntry entry;

    file_id_ctx.htable.capacity = 1403641;
    bytes = sizeof(FileIdInfo *) * file_id_ctx.htable.capacity;
    file_id_ctx.htable.buckets = fc_malloc(bytes);
    if (file_id_ctx.htable.buckets == NULL) {
        return ENOMEM;
    }
    memset(file_id_ctx.htable.buckets, 0, bytes);

    file_id_ctx.lock_array.count = 163;
    bytes = sizeof(pthread_mutex_t) * file_id_ctx.lock_array.count;
    file_id_ctx.lock_array.locks = fc_malloc(bytes);
    if (file_id_ctx.lock_array.locks == NULL) {
        return ENOMEM;
    }

    end = file_id_ctx.lock_array.locks + file_id_ctx.lock_array.count;
    for (lock=file_id_ctx.lock_array.locks; lock<end; lock++) {
        if ((result=init_pthread_lock(lock)) != 0) {
            return result;
        }
    }

    if ((result=init_pthread_lock(&file_id_ctx.list.lock)) != 0) {
        return result;
    }

    if ((result=fast_mblock_init_ex1(&file_id_ctx.allocator,
                    "file-id", sizeof(FileIdInfo), 16 * 1024,
                    0, NULL, NULL, true)) != 0)
    {
        return result;
    }

    FAST_ALLOCATOR_INIT_REGION(regions[0],  0,  48, 48, 16 * 1024);
    FAST_ALLOCATOR_INIT_REGION(regions[1], 48, 128,  8,  8 * 1024);
    if ((result=fast_allocator_init_ex(&file_id_ctx.acontext, "file-id",
                    obj_size, NULL, regions, 2, 0, 0.00, 0, true)) != 0)
    {
        return result;
    }

    INIT_SCHEDULE_ENTRY(entry, FDFS_CLEAR_EXPIRED_FILE_ID_TASK_ID,
            0, 0, 0, 1, clear_expired_file_id_func, NULL);
    scheduleArray.count = 1;
    scheduleArray.entries = &entry;
    return sched_add_entries(&scheduleArray);
}

void file_id_hashtable_destroy()
{
    if (file_id_ctx.htable.buckets != NULL) {
        free(file_id_ctx.htable.buckets);
        file_id_ctx.htable.buckets = NULL;
    }

    if (file_id_ctx.lock_array.locks != NULL) {
        pthread_mutex_t *lock;
        pthread_mutex_t *end;

        end = file_id_ctx.lock_array.locks +
            file_id_ctx.lock_array.count;
        for (lock=file_id_ctx.lock_array.locks; lock<end; lock++) {
            pthread_mutex_destroy(lock);
        }

        free(file_id_ctx.lock_array.locks);
        file_id_ctx.lock_array.locks = NULL;
    }

    fast_mblock_destroy(&file_id_ctx.allocator);
    pthread_mutex_destroy(&file_id_ctx.list.lock);
    fast_allocator_destroy(&file_id_ctx.acontext);
}

#define FILE_ID_HASHTABLE_DECLARE_VARS() \
    uint32_t bucket_index;  \
    FileIdInfo **bucket;    \
    pthread_mutex_t *lock

#define FILE_ID_HASHTABLE_SET_BUCKET_AND_LOCK(hash_code) \
    bucket_index = hash_code % file_id_ctx.htable.capacity; \
    bucket = file_id_ctx.htable.buckets + bucket_index;    \
    lock = file_id_ctx.lock_array.locks + bucket_index %   \
        file_id_ctx.lock_array.count

int file_id_hashtable_add(const string_t *file_id)
{
    int result;
    uint32_t hash_code;
    FileIdInfo *current;
    FileIdInfo *previous;
    FileIdInfo *finfo;
    FILE_ID_HASHTABLE_DECLARE_VARS();

    hash_code = fc_simple_hash(file_id->str, file_id->len);
    FILE_ID_HASHTABLE_SET_BUCKET_AND_LOCK(hash_code);

    result = 0;
    PTHREAD_MUTEX_LOCK(lock);
    previous = NULL;
    current = *bucket;
    while (current != NULL) {
        if (hash_code < current->hash_code) {
            break;
        } else if (hash_code == current->hash_code && fc_string_equal(
                    file_id, &current->file_id))
        {
            result = EEXIST;
            break;
        }

        previous = current;
        current = current->nexts.htable;
    }

    if (result == 0) {
        do {
            if ((finfo=fast_mblock_alloc_object(&file_id_ctx.
                            allocator)) == NULL)
            {
                result = ENOMEM;
                break;
            }
            if ((finfo->file_id.str=fast_allocator_alloc(&file_id_ctx.
                            acontext, file_id->len)) == NULL)
            {
                fast_mblock_free_object(&file_id_ctx.allocator, finfo);
                result = ENOMEM;
                break;
            }

            memcpy(finfo->file_id.str, file_id->str, file_id->len);
            finfo->file_id.len = file_id->len;
            finfo->hash_code = hash_code;
            finfo->expires = g_current_time + 3;
            if (previous == NULL) {
                finfo->nexts.htable = *bucket;
                *bucket = finfo;
            } else {
                finfo->nexts.htable = current;
                previous->nexts.htable = finfo;
            }

#if defined(DEBUG_FLAG)
            FC_ATOMIC_INC(file_id_ctx.htable.count);
#endif

        } while (0);
    } else {
        finfo = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(lock);

    if (result == 0) {
        PTHREAD_MUTEX_LOCK(&file_id_ctx.list.lock);
        finfo->nexts.list = NULL;
        if (file_id_ctx.list.tail == NULL) {
            file_id_ctx.list.head = finfo;
        } else {
            file_id_ctx.list.tail->nexts.list = finfo;
        }
        file_id_ctx.list.tail = finfo;
        PTHREAD_MUTEX_UNLOCK(&file_id_ctx.list.lock);
    }
    return result;
}

static int file_id_hashtable_del(FileIdInfo *finfo)
{
    int result;
    FileIdInfo *current;
    FileIdInfo *previous;
    FILE_ID_HASHTABLE_DECLARE_VARS();

    FILE_ID_HASHTABLE_SET_BUCKET_AND_LOCK(finfo->hash_code);
    PTHREAD_MUTEX_LOCK(lock);
    if (*bucket == NULL) {
        result = ENOENT;
    } else if (finfo->hash_code == (*bucket)->hash_code &&
            fc_string_equal(&finfo->file_id, &(*bucket)->file_id))
    {
        *bucket = (*bucket)->nexts.htable;
#if defined(DEBUG_FLAG)
        FC_ATOMIC_DEC(file_id_ctx.htable.count);
#endif
        result = 0;
    } else {
        result = ENOENT;
        previous = *bucket;
        while ((current=previous->nexts.htable) != NULL) {
            if (finfo->hash_code < current->hash_code) {
                break;
            } else if (finfo->hash_code == current->hash_code &&
                    fc_string_equal(&finfo->file_id, &current->file_id))
            {
                previous->nexts.htable = current->nexts.htable;
#if defined(DEBUG_FLAG)
                FC_ATOMIC_DEC(file_id_ctx.htable.count);
#endif
                result = 0;
                break;
            }

            previous = current;
        }
    }
    PTHREAD_MUTEX_UNLOCK(lock);

    return result;
}

static int clear_expired_file_id_func(void *args)
{
    struct file_id_info *head;
    struct file_id_info *tail;
    struct fast_mblock_chain chain;
    struct fast_mblock_node *node;

    head = tail = NULL;
    PTHREAD_MUTEX_LOCK(&file_id_ctx.list.lock);
    if (file_id_ctx.list.head != NULL && file_id_ctx.
            list.head->expires < g_current_time)
    {
        head = tail = file_id_ctx.list.head;
        file_id_ctx.list.head = file_id_ctx.list.head->nexts.list;
        while (file_id_ctx.list.head != NULL && file_id_ctx.
                list.head->expires < g_current_time)
        {
            tail = file_id_ctx.list.head;
            file_id_ctx.list.head = file_id_ctx.list.head->nexts.list;
        }

        if (file_id_ctx.list.head == NULL) {
            file_id_ctx.list.tail = NULL;
        } else {
            tail->nexts.list = NULL;
        }
    }
    PTHREAD_MUTEX_UNLOCK(&file_id_ctx.list.lock);

    if (head == NULL) {
        return 0;
    }

    chain.head = chain.tail = NULL;
    do {
        node = fast_mblock_to_node_ptr(head);
        if (chain.head == NULL) {
            chain.head = node;
        } else {
            chain.tail->next = node;
        }
        chain.tail = node;

        file_id_hashtable_del(head);
        fast_allocator_free(&file_id_ctx.acontext, head->file_id.str);
    } while ((head=head->nexts.list) != NULL);

    chain.tail->next = NULL;
    fast_mblock_batch_free(&file_id_ctx.allocator, &chain);
    return 0;
}
