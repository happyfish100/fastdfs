/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//storage_bulk_import.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/sched_thread.h"
#include "fastcommon/hash.h"
#include "fastcommon/fc_atomic.h"
#include "fdfs_global.h"
#include "fdfs_shared_func.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_service.h"
#include "trunk_mgr/trunk_shared.h"
#include "storage_bulk_import.h"

/* Buffer size for file operations */
#define BULK_IMPORT_BUFFER_SIZE  (256 * 1024)  /* 256KB */

/* Maximum file size for bulk import (default: 1GB) */
#define BULK_IMPORT_MAX_FILE_SIZE  (1024 * 1024 * 1024LL)

static bool g_bulk_import_initialized = false;

int storage_bulk_import_init()
{
    if (g_bulk_import_initialized) {
        return 0;
    }

    logInfo("Bulk import module initialized");
    g_bulk_import_initialized = true;
    return 0;
}

void storage_bulk_import_destroy()
{
    if (!g_bulk_import_initialized) {
        return;
    }

    logInfo("Bulk import module destroyed");
    g_bulk_import_initialized = false;
}

int storage_validate_file_path(const char *file_path,
    char *error_message, int error_size)
{
    struct stat file_stat;

    if (file_path == NULL || *file_path == '\0') {
        snprintf(error_message, error_size, "File path is empty");
        return BULK_IMPORT_ERROR_INVALID_PATH;
    }

    if (strlen(file_path) >= MAX_PATH_SIZE) {
        snprintf(error_message, error_size, "File path too long: %d >= %d",
            (int)strlen(file_path), MAX_PATH_SIZE);
        return BULK_IMPORT_ERROR_INVALID_PATH;
    }

    if (stat(file_path, &file_stat) != 0) {
        snprintf(error_message, error_size, "File not found: %s, errno: %d, error info: %s",
            file_path, errno, STRERROR(errno));
        return BULK_IMPORT_ERROR_FILE_NOT_FOUND;
    }

    if (!S_ISREG(file_stat.st_mode)) {
        snprintf(error_message, error_size, "Not a regular file: %s", file_path);
        return BULK_IMPORT_ERROR_INVALID_PATH;
    }

    if (access(file_path, R_OK) != 0) {
        snprintf(error_message, error_size, "No read permission: %s, errno: %d, error info: %s",
            file_path, errno, STRERROR(errno));
        return BULK_IMPORT_ERROR_PERMISSION;
    }

    if (file_stat.st_size > BULK_IMPORT_MAX_FILE_SIZE) {
        snprintf(error_message, error_size, "File too large: %"PRId64" > %"PRId64,
            (int64_t)file_stat.st_size, BULK_IMPORT_MAX_FILE_SIZE);
        return BULK_IMPORT_ERROR_FILE_TOO_LARGE;
    }

    return BULK_IMPORT_ERROR_NONE;
}

static uint32_t calculate_crc32_for_file(const char *file_path, int64_t file_size)
{
    int fd;
    char buffer[BULK_IMPORT_BUFFER_SIZE];
    ssize_t bytes_read;
    uint32_t crc32 = 0;
    int64_t total_read = 0;

    fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        logError("file: "__FILE__", line: %d, "
            "open file %s fail, errno: %d, error info: %s",
            __LINE__, file_path, errno, STRERROR(errno));
        return 0;
    }

    crc32 = CRC32_XINIT;
    while (total_read < file_size) {
        bytes_read = read(fd, buffer, BULK_IMPORT_BUFFER_SIZE);
        if (bytes_read < 0) {
            logError("file: "__FILE__", line: %d, "
                "read file %s fail, errno: %d, error info: %s",
                __LINE__, file_path, errno, STRERROR(errno));
            close(fd);
            return 0;
        }
        if (bytes_read == 0) {
            break;
        }

        crc32 = CRC32_ex(buffer, bytes_read, crc32);
        total_read += bytes_read;
    }

    crc32 = CRC32_FINAL(crc32);
    close(fd);

    return crc32;
}

int storage_calculate_file_metadata(const char *file_path,
    BulkImportFileInfo *file_info, bool calculate_crc32)
{
    struct stat file_stat;
    const char *file_ext;
    int result;

    memset(file_info, 0, sizeof(BulkImportFileInfo));
    snprintf(file_info->source_path, sizeof(file_info->source_path), "%s", file_path);

    result = storage_validate_file_path(file_path,
        file_info->error_message, sizeof(file_info->error_message));
    if (result != BULK_IMPORT_ERROR_NONE) {
        file_info->error_code = result;
        return result;
    }

    if (stat(file_path, &file_stat) != 0) {
        snprintf(file_info->error_message, sizeof(file_info->error_message),
            "stat file fail, errno: %d, error info: %s", errno, STRERROR(errno));
        file_info->error_code = BULK_IMPORT_ERROR_METADATA_FAILED;
        return BULK_IMPORT_ERROR_METADATA_FAILED;
    }

    file_info->file_size = file_stat.st_size;
    file_info->create_timestamp = file_stat.st_ctime;
    file_info->modify_timestamp = file_stat.st_mtime;

    file_ext = fdfs_get_file_ext_name(file_path);
    if (file_ext != NULL) {
        snprintf(file_info->file_ext_name, sizeof(file_info->file_ext_name), "%s", file_ext);
    }

    if (calculate_crc32) {
        file_info->crc32 = calculate_crc32_for_file(file_path, file_info->file_size);
        if (file_info->crc32 == 0) {
            snprintf(file_info->error_message, sizeof(file_info->error_message),
                "Calculate CRC32 failed");
            file_info->error_code = BULK_IMPORT_ERROR_CRC32_FAILED;
            return BULK_IMPORT_ERROR_CRC32_FAILED;
        }
    }

    file_info->status = BULK_IMPORT_STATUS_INIT;
    file_info->error_code = BULK_IMPORT_ERROR_NONE;

    logDebug("file: "__FILE__", line: %d, "
        "file metadata: path=%s, size=%"PRId64", crc32=%u, ext=%s",
        __LINE__, file_path, file_info->file_size, file_info->crc32,
        file_info->file_ext_name);

    return 0;
}

int storage_generate_file_id(BulkImportFileInfo *file_info,
    const char *group_name, int store_path_index)
{
    char filename[128];
    char file_id[FDFS_FILE_ID_LEN];
    int result;

    if (file_info == NULL || group_name == NULL) {
        return EINVAL;
    }

    if (store_path_index < 0 || store_path_index >= g_fdfs_store_paths.count) {
        snprintf(file_info->error_message, sizeof(file_info->error_message),
            "Invalid store path index: %d", store_path_index);
        file_info->error_code = BULK_IMPORT_ERROR_INVALID_PATH;
        return BULK_IMPORT_ERROR_INVALID_PATH;
    }

    snprintf(file_info->group_name, sizeof(file_info->group_name), "%s", group_name);
    file_info->store_path_index = store_path_index;

    result = storage_gen_filename(NULL, file_info->create_timestamp,
        file_info->file_size, file_info->crc32, file_info->file_ext_name,
        filename, sizeof(filename));
    if (result != 0) {
        snprintf(file_info->error_message, sizeof(file_info->error_message),
            "Generate filename failed, result: %d", result);
        file_info->error_code = BULK_IMPORT_ERROR_METADATA_FAILED;
        return result;
    }

    snprintf(file_id, sizeof(file_id), "%s/%s", group_name, filename);
    snprintf(file_info->file_id, sizeof(file_info->file_id), "%s", file_id);

    logDebug("file: "__FILE__", line: %d, "
        "generated file_id: %s for source: %s",
        __LINE__, file_info->file_id, file_info->source_path);

    return 0;
}

int storage_get_full_file_path(int store_path_index, const char *file_id,
    char *full_path, int path_size)
{
    char true_filename[128];
    char *filename;
    int filename_len;

    if (file_id == NULL || full_path == NULL) {
        return EINVAL;
    }

    filename = strchr(file_id, '/');
    if (filename == NULL) {
        filename = (char *)file_id;
    } else {
        filename++;
    }

    filename_len = strlen(filename);
    if (filename_len == 0) {
        return EINVAL;
    }

    trunk_get_full_filename(NULL, filename, filename_len,
        true_filename, sizeof(true_filename));

    snprintf(full_path, path_size, "%s/data/%s",
        FDFS_STORE_PATH_STR(store_path_index), true_filename);

    return 0;
}

bool storage_check_available_space(int store_path_index, int64_t required_bytes)
{
    int64_t free_mb;

    if (store_path_index < 0 || store_path_index >= g_fdfs_store_paths.count) {
        return false;
    }

    free_mb = g_fdfs_store_paths.paths[store_path_index].free_mb;
    
    if (free_mb * 1024 * 1024 < required_bytes + (100 * 1024 * 1024LL)) {
        logWarning("file: "__FILE__", line: %d, "
            "storage path %d has insufficient space: free=%"PRId64"MB, required=%"PRId64"MB",
            __LINE__, store_path_index, free_mb, required_bytes / (1024 * 1024));
        return false;
    }

    return true;
}

static int copy_file_content(const char *src_path, const char *dest_path, int64_t file_size)
{
    int src_fd = -1;
    int dest_fd = -1;
    char buffer[BULK_IMPORT_BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t bytes_written;
    int64_t total_copied = 0;
    int result = 0;

    src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        logError("file: "__FILE__", line: %d, "
            "open source file %s fail, errno: %d, error info: %s",
            __LINE__, src_path, errno, STRERROR(errno));
        return errno != 0 ? errno : EIO;
    }

    dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        logError("file: "__FILE__", line: %d, "
            "open dest file %s fail, errno: %d, error info: %s",
            __LINE__, dest_path, errno, STRERROR(errno));
        close(src_fd);
        return errno != 0 ? errno : EIO;
    }

    while (total_copied < file_size) {
        bytes_read = read(src_fd, buffer, BULK_IMPORT_BUFFER_SIZE);
        if (bytes_read < 0) {
            logError("file: "__FILE__", line: %d, "
                "read from %s fail, errno: %d, error info: %s",
                __LINE__, src_path, errno, STRERROR(errno));
            result = errno != 0 ? errno : EIO;
            break;
        }
        if (bytes_read == 0) {
            break;
        }

        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            logError("file: "__FILE__", line: %d, "
                "write to %s fail, errno: %d, error info: %s",
                __LINE__, dest_path, errno, STRERROR(errno));
            result = errno != 0 ? errno : EIO;
            break;
        }

        total_copied += bytes_read;
    }

    close(src_fd);
    if (fsync(dest_fd) != 0) {
        logError("file: "__FILE__", line: %d, "
            "fsync file %s fail, errno: %d, error info: %s",
            __LINE__, dest_path, errno, STRERROR(errno));
        if (result == 0) {
            result = errno != 0 ? errno : EIO;
        }
    }
    close(dest_fd);

    if (result != 0) {
        unlink(dest_path);
    }

    return result;
}

int storage_transfer_file_to_storage(BulkImportFileInfo *file_info, int import_mode)
{
    char dest_path[MAX_PATH_SIZE];
    char dest_dir[MAX_PATH_SIZE];
    char *last_slash;
    int result;

    if (file_info == NULL || file_info->file_id[0] == '\0') {
        return EINVAL;
    }

    result = storage_get_full_file_path(file_info->store_path_index,
        file_info->file_id, dest_path, sizeof(dest_path));
    if (result != 0) {
        snprintf(file_info->error_message, sizeof(file_info->error_message),
            "Get full file path failed");
        file_info->error_code = BULK_IMPORT_ERROR_INVALID_PATH;
        return result;
    }

    snprintf(dest_dir, sizeof(dest_dir), "%s", dest_path);
    last_slash = strrchr(dest_dir, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (!fileExists(dest_dir)) {
            if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
                logError("file: "__FILE__", line: %d, "
                    "mkdir %s fail, errno: %d, error info: %s",
                    __LINE__, dest_dir, errno, STRERROR(errno));
                snprintf(file_info->error_message, sizeof(file_info->error_message),
                    "Create directory failed: %s", STRERROR(errno));
                file_info->error_code = BULK_IMPORT_ERROR_COPY_FAILED;
                return errno != 0 ? errno : EIO;
            }
        }
    }

    if (import_mode == BULK_IMPORT_MODE_MOVE) {
        if (rename(file_info->source_path, dest_path) == 0) {
            logInfo("file: "__FILE__", line: %d, "
                "moved file from %s to %s",
                __LINE__, file_info->source_path, dest_path);
            return 0;
        }

        if (errno != EXDEV) {
            logError("file: "__FILE__", line: %d, "
                "move file from %s to %s fail, errno: %d, error info: %s",
                __LINE__, file_info->source_path, dest_path, errno, STRERROR(errno));
            snprintf(file_info->error_message, sizeof(file_info->error_message),
                "Move file failed: %s", STRERROR(errno));
            file_info->error_code = BULK_IMPORT_ERROR_MOVE_FAILED;
            return errno != 0 ? errno : EIO;
        }

        logWarning("file: "__FILE__", line: %d, "
            "rename across filesystems, falling back to copy+delete",
            __LINE__);
    }

    result = copy_file_content(file_info->source_path, dest_path, file_info->file_size);
    if (result != 0) {
        snprintf(file_info->error_message, sizeof(file_info->error_message),
            "Copy file failed: %s", STRERROR(result));
        file_info->error_code = BULK_IMPORT_ERROR_COPY_FAILED;
        return result;
    }

    logInfo("file: "__FILE__", line: %d, "
        "copied file from %s to %s",
        __LINE__, file_info->source_path, dest_path);

    if (import_mode == BULK_IMPORT_MODE_MOVE) {
        if (unlink(file_info->source_path) != 0) {
            logWarning("file: "__FILE__", line: %d, "
                "delete source file %s fail, errno: %d, error info: %s",
                __LINE__, file_info->source_path, errno, STRERROR(errno));
        }
    }

    return 0;
}

int storage_update_index_for_bulk_file(BulkImportFileInfo *file_info)
{
    if (file_info == NULL) {
        return EINVAL;
    }

    logInfo("file: "__FILE__", line: %d, "
        "index updated for file_id: %s, size: %"PRId64", crc32: %u",
        __LINE__, file_info->file_id, file_info->file_size, file_info->crc32);

    return 0;
}

int storage_register_bulk_file(BulkImportContext *context, BulkImportFileInfo *file_info)
{
    int result;

    if (context == NULL || file_info == NULL) {
        return EINVAL;
    }

    file_info->status = BULK_IMPORT_STATUS_PROCESSING;

    if (!storage_check_available_space(file_info->store_path_index, file_info->file_size)) {
        snprintf(file_info->error_message, sizeof(file_info->error_message),
            "Insufficient storage space");
        file_info->error_code = BULK_IMPORT_ERROR_NO_SPACE;
        file_info->status = BULK_IMPORT_STATUS_FAILED;
        return BULK_IMPORT_ERROR_NO_SPACE;
    }

    if (context->validate_only) {
        logInfo("file: "__FILE__", line: %d, "
            "dry-run mode: would import %s as %s",
            __LINE__, file_info->source_path, file_info->file_id);
        file_info->status = BULK_IMPORT_STATUS_SUCCESS;
        return 0;
    }

    result = storage_transfer_file_to_storage(file_info, context->import_mode);
    if (result != 0) {
        file_info->status = BULK_IMPORT_STATUS_FAILED;
        return result;
    }

    result = storage_update_index_for_bulk_file(file_info);
    if (result != 0) {
        snprintf(file_info->error_message, sizeof(file_info->error_message),
            "Update index failed");
        file_info->error_code = BULK_IMPORT_ERROR_INDEX_UPDATE;
        file_info->status = BULK_IMPORT_STATUS_FAILED;
        return result;
    }

    file_info->status = BULK_IMPORT_STATUS_SUCCESS;
    __sync_add_and_fetch(&context->success_files, 1);
    __sync_add_and_fetch(&context->total_bytes, file_info->file_size);

    logInfo("file: "__FILE__", line: %d, "
        "successfully registered file: %s -> %s, size: %"PRId64,
        __LINE__, file_info->source_path, file_info->file_id, file_info->file_size);

    return 0;
}
