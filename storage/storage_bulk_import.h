/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//storage_bulk_import.h

#ifndef _STORAGE_BULK_IMPORT_H_
#define _STORAGE_BULK_IMPORT_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fastcommon/common_define.h"
#include "fastcommon/logger.h"
#include "fdfs_define.h"
#include "fdfs_global.h"
#include "tracker_types.h"

/* Import modes */
#define BULK_IMPORT_MODE_COPY    0  /* Copy files to storage path */
#define BULK_IMPORT_MODE_MOVE    1  /* Move files to storage path */

/* Import status codes */
#define BULK_IMPORT_STATUS_INIT       0
#define BULK_IMPORT_STATUS_PROCESSING 1
#define BULK_IMPORT_STATUS_SUCCESS    2
#define BULK_IMPORT_STATUS_FAILED     3
#define BULK_IMPORT_STATUS_SKIPPED    4

/* Error codes */
#define BULK_IMPORT_ERROR_NONE              0
#define BULK_IMPORT_ERROR_FILE_NOT_FOUND    1
#define BULK_IMPORT_ERROR_FILE_TOO_LARGE    2
#define BULK_IMPORT_ERROR_INVALID_PATH      3
#define BULK_IMPORT_ERROR_METADATA_FAILED   4
#define BULK_IMPORT_ERROR_COPY_FAILED       5
#define BULK_IMPORT_ERROR_MOVE_FAILED       6
#define BULK_IMPORT_ERROR_INDEX_UPDATE      7
#define BULK_IMPORT_ERROR_CRC32_FAILED      8
#define BULK_IMPORT_ERROR_NO_SPACE          9
#define BULK_IMPORT_ERROR_PERMISSION        10

#ifdef __cplusplus
extern "C" {
#endif

/* File metadata structure */
typedef struct {
    char source_path[MAX_PATH_SIZE];      /* Original file path */
    char file_id[FDFS_FILE_ID_LEN];       /* Generated FastDFS file ID */
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int64_t file_size;                     /* File size in bytes */
    uint32_t crc32;                        /* CRC32 checksum */
    time_t create_timestamp;               /* File creation time */
    time_t modify_timestamp;               /* File modification time */
    char file_ext_name[FDFS_FILE_EXT_NAME_MAX_LEN + 1];
    int store_path_index;                  /* Storage path index */
    int status;                            /* Import status */
    int error_code;                        /* Error code if failed */
    char error_message[256];               /* Error message */
} BulkImportFileInfo;

/* Bulk import context */
typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int store_path_index;                  /* Target storage path */
    int import_mode;                       /* COPY or MOVE */
    bool calculate_crc32;                  /* Whether to calculate CRC32 */
    bool validate_only;                    /* Dry-run mode */
    int64_t total_files;                   /* Total files to import */
    int64_t processed_files;               /* Files processed */
    int64_t success_files;                 /* Successfully imported */
    int64_t failed_files;                  /* Failed imports */
    int64_t skipped_files;                 /* Skipped files */
    int64_t total_bytes;                   /* Total bytes imported */
    time_t start_time;                     /* Import start time */
    time_t end_time;                       /* Import end time */
} BulkImportContext;

/**
 * Initialize bulk import module
 * @return 0 for success, error code otherwise
 */
int storage_bulk_import_init();

/**
 * Destroy bulk import module
 */
void storage_bulk_import_destroy();

/**
 * Calculate file metadata (size, CRC32, timestamps)
 * @param file_path: source file path
 * @param file_info: output file metadata
 * @param calculate_crc32: whether to calculate CRC32 checksum
 * @return 0 for success, error code otherwise
 */
int storage_calculate_file_metadata(const char *file_path,
    BulkImportFileInfo *file_info, bool calculate_crc32);

/**
 * Generate FastDFS file ID for the file
 * @param file_info: file metadata
 * @param group_name: storage group name
 * @param store_path_index: storage path index
 * @return 0 for success, error code otherwise
 */
int storage_generate_file_id(BulkImportFileInfo *file_info,
    const char *group_name, int store_path_index);

/**
 * Register file in FastDFS storage without upload
 * @param context: bulk import context
 * @param file_info: file metadata
 * @return 0 for success, error code otherwise
 */
int storage_register_bulk_file(BulkImportContext *context,
    BulkImportFileInfo *file_info);

/**
 * Copy or move file to storage path
 * @param file_info: file metadata with file_id
 * @param import_mode: COPY or MOVE
 * @return 0 for success, error code otherwise
 */
int storage_transfer_file_to_storage(BulkImportFileInfo *file_info,
    int import_mode);

/**
 * Update storage index with imported file
 * @param file_info: file metadata
 * @return 0 for success, error code otherwise
 */
int storage_update_index_for_bulk_file(BulkImportFileInfo *file_info);

/**
 * Validate file path and permissions
 * @param file_path: file path to validate
 * @param error_message: output error message buffer
 * @param error_size: error message buffer size
 * @return 0 for success, error code otherwise
 */
int storage_validate_file_path(const char *file_path,
    char *error_message, int error_size);

/**
 * Get storage path for file
 * @param store_path_index: storage path index
 * @param file_id: FastDFS file ID
 * @param full_path: output full file path buffer
 * @param path_size: buffer size
 * @return 0 for success, error code otherwise
 */
int storage_get_full_file_path(int store_path_index, const char *file_id,
    char *full_path, int path_size);

/**
 * Check if storage path has enough space
 * @param store_path_index: storage path index
 * @param required_bytes: required space in bytes
 * @return true if enough space, false otherwise
 */
bool storage_check_available_space(int store_path_index,
    int64_t required_bytes);

#ifdef __cplusplus
}
#endif

#endif
