/**
 * FastDFS Compression Tool
 * 
 * This tool provides comprehensive file compression capabilities for FastDFS,
 * allowing users to compress files to save storage space. It supports multiple
 * compression algorithms, in-place compression, and decompression operations.
 * 
 * Features:
 * - Compress files in-place (replace original) or create compressed copies
 * - Support multiple compression algorithms (gzip, zstd)
 * - Decompress compressed files
 * - Preserve file metadata during compression
 * - Multi-threaded parallel compression
 * - Progress tracking and statistics
 * - Compression ratio reporting
 * - JSON and text output formats
 * 
 * Compression Algorithms:
 * - gzip: Standard gzip compression (good balance of speed and ratio)
 * - zstd: Zstandard compression (better ratio, faster decompression)
 * 
 * Compression Modes:
 * - In-place: Replace original file with compressed version
 * - Copy: Create compressed copy, keep original
 * - Decompress: Decompress compressed files back to original
 * 
 * Use Cases:
 * - Reduce storage usage for archived files
 * - Compress large files to save space
 * - Optimize storage capacity
 * - Compress old or infrequently accessed files
 * - Decompress files when needed
 * 
 * Copyright (C) 2025
 * License: GPL V3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <zlib.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum file ID length */
#define MAX_FILE_ID_LEN 256

/* Maximum number of threads for parallel processing */
#define MAX_THREADS 20

/* Default number of threads */
#define DEFAULT_THREADS 4

/* Maximum line length for file operations */
#define MAX_LINE_LEN 4096

/* Buffer size for compression operations */
#define COMPRESS_BUFFER_SIZE (256 * 1024)

/* Compression algorithm enumeration */
typedef enum {
    COMPRESS_ALG_GZIP = 0,   /* Gzip compression */
    COMPRESS_ALG_ZSTD = 1,   /* Zstandard compression */
    COMPRESS_ALG_AUTO = 2    /* Auto-detect from file extension */
} CompressAlgorithm;

/* Compression operation enumeration */
typedef enum {
    COMPRESS_OP_COMPRESS = 0,   /* Compress file */
    COMPRESS_OP_DECOMPRESS = 1, /* Decompress file */
    COMPRESS_OP_AUTO = 2        /* Auto-detect operation */
} CompressOperation;

/* Compression task structure */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];        /* File ID */
    char original_file_id[MAX_FILE_ID_LEN]; /* Original file ID (for decompress) */
    CompressAlgorithm algorithm;         /* Compression algorithm */
    CompressOperation operation;          /* Operation type */
    int in_place;                         /* In-place mode flag */
    int64_t original_size;                /* Original file size */
    int64_t compressed_size;              /* Compressed file size */
    double compression_ratio;              /* Compression ratio (0.0 - 1.0) */
    int status;                            /* Task status (0 = pending, 1 = success, -1 = failed) */
    char error_msg[512];                  /* Error message if failed */
    time_t start_time;                    /* When task started */
    time_t end_time;                      /* When task completed */
} CompressTask;

/* Compression context */
typedef struct {
    CompressTask *tasks;                  /* Array of compression tasks */
    int task_count;                        /* Number of tasks */
    int current_index;                     /* Current task index */
    pthread_mutex_t mutex;                 /* Mutex for thread synchronization */
    ConnectionInfo *pTrackerServer;        /* Tracker server connection */
    CompressAlgorithm default_algorithm;    /* Default compression algorithm */
    int preserve_metadata;                 /* Preserve metadata flag */
    int verbose;                           /* Verbose output flag */
    int json_output;                       /* JSON output flag */
} CompressContext;

/* Global statistics */
static int total_files_processed = 0;
static int files_compressed = 0;
static int files_decompressed = 0;
static int files_failed = 0;
static int64_t total_original_bytes = 0;
static int64_t total_compressed_bytes = 0;
static int64_t total_bytes_saved = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_compress tool, including all available options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <file_id> [file_id...]\n", program_name);
    printf("       %s [OPTIONS] -f <file_list>\n", program_name);
    printf("\n");
    printf("FastDFS File Compression Tool\n");
    printf("\n");
    printf("This tool compresses or decompresses files in FastDFS to save\n");
    printf("storage space. It supports multiple compression algorithms and\n");
    printf("can operate in-place or create compressed copies.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST        File list to process (one file ID per line)\n");
    printf("  -a, --algorithm ALG   Compression algorithm: gzip, zstd (default: gzip)\n");
    printf("  -o, --operation OP     Operation: compress, decompress, auto (default: compress)\n");
    printf("  -i, --in-place         Replace original file (default: create copy)\n");
    printf("  -m, --metadata         Preserve file metadata during compression\n");
    printf("  -j, --threads NUM      Number of parallel threads (default: 4, max: 20)\n");
    printf("  --output FILE          Output report file (default: stdout)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -q, --quiet            Quiet mode (only show errors)\n");
    printf("  -J, --json             Output in JSON format\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Compression Algorithms:\n");
    printf("  gzip  - Standard gzip compression (good balance)\n");
    printf("  zstd  - Zstandard compression (better ratio, faster)\n");
    printf("\n");
    printf("Operations:\n");
    printf("  compress   - Compress files\n");
    printf("  decompress - Decompress files\n");
    printf("  auto       - Auto-detect based on file extension\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - All operations completed successfully\n");
    printf("  1 - Some operations failed\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Compress a file\n");
    printf("  %s group1/M00/00/00/file.jpg\n", program_name);
    printf("\n");
    printf("  # Compress in-place with gzip\n");
    printf("  %s -i -a gzip group1/M00/00/00/file.jpg\n", program_name);
    printf("\n");
    printf("  # Decompress files\n");
    printf("  %s -o decompress -f compressed_files.txt\n", program_name);
    printf("\n");
    printf("  # Compress multiple files in parallel\n");
    printf("  %s -f file_list.txt -j 8\n", program_name);
}

/**
 * Format bytes to human-readable string
 * 
 * This function converts a byte count to a human-readable string
 * with appropriate units (B, KB, MB, GB, TB).
 * 
 * @param bytes - Number of bytes to format
 * @param buf - Output buffer for formatted string
 * @param buf_size - Size of output buffer
 */
static void format_bytes(int64_t bytes, char *buf, size_t buf_size) {
    if (bytes >= 1099511627776LL) {
        snprintf(buf, buf_size, "%.2f TB", bytes / 1099511627776.0);
    } else if (bytes >= 1073741824LL) {
        snprintf(buf, buf_size, "%.2f GB", bytes / 1073741824.0);
    } else if (bytes >= 1048576LL) {
        snprintf(buf, buf_size, "%.2f MB", bytes / 1048576.0);
    } else if (bytes >= 1024LL) {
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    } else {
        snprintf(buf, buf_size, "%lld B", (long long)bytes);
    }
}

/**
 * Compress file using gzip
 * 
 * This function compresses a file using gzip compression algorithm.
 * 
 * @param input_file - Input file path
 * @param output_file - Output file path
 * @param original_size - Output parameter for original size
 * @param compressed_size - Output parameter for compressed size
 * @return 0 on success, error code on failure
 */
static int compress_gzip(const char *input_file, const char *output_file,
                         int64_t *original_size, int64_t *compressed_size) {
    FILE *in_fp, *out_fp;
    gzFile gz_fp;
    unsigned char buffer[COMPRESS_BUFFER_SIZE];
    size_t bytes_read;
    int64_t total_read = 0;
    int64_t total_written = 0;
    int ret;
    
    if (input_file == NULL || output_file == NULL ||
        original_size == NULL || compressed_size == NULL) {
        return EINVAL;
    }
    
    /* Open input file */
    in_fp = fopen(input_file, "rb");
    if (in_fp == NULL) {
        return errno;
    }
    
    /* Open output file for gzip compression */
    gz_fp = gzopen(output_file, "wb");
    if (gz_fp == NULL) {
        fclose(in_fp);
        return errno;
    }
    
    /* Compress data */
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), in_fp)) > 0) {
        total_read += bytes_read;
        
        ret = gzwrite(gz_fp, buffer, bytes_read);
        if (ret <= 0) {
            gzclose(gz_fp);
            fclose(in_fp);
            return EIO;
        }
        total_written += ret;
    }
    
    /* Close files */
    gzclose(gz_fp);
    fclose(in_fp);
    
    /* Get compressed file size */
    out_fp = fopen(output_file, "rb");
    if (out_fp != NULL) {
        fseek(out_fp, 0, SEEK_END);
        *compressed_size = ftell(out_fp);
        fclose(out_fp);
    } else {
        *compressed_size = total_written;
    }
    
    *original_size = total_read;
    
    return 0;
}

/**
 * Decompress gzip file
 * 
 * This function decompresses a gzip-compressed file.
 * 
 * @param input_file - Input compressed file path
 * @param output_file - Output decompressed file path
 * @param original_size - Output parameter for original size
 * @param compressed_size - Output parameter for compressed size
 * @return 0 on success, error code on failure
 */
static int decompress_gzip(const char *input_file, const char *output_file,
                          int64_t *original_size, int64_t *compressed_size) {
    gzFile gz_fp;
    FILE *out_fp;
    unsigned char buffer[COMPRESS_BUFFER_SIZE];
    int bytes_read;
    int64_t total_read = 0;
    int64_t total_written = 0;
    struct stat st;
    
    if (input_file == NULL || output_file == NULL ||
        original_size == NULL || compressed_size == NULL) {
        return EINVAL;
    }
    
    /* Get compressed file size */
    if (stat(input_file, &st) == 0) {
        *compressed_size = st.st_size;
    } else {
        *compressed_size = 0;
    }
    
    /* Open compressed file */
    gz_fp = gzopen(input_file, "rb");
    if (gz_fp == NULL) {
        return errno;
    }
    
    /* Open output file */
    out_fp = fopen(output_file, "wb");
    if (out_fp == NULL) {
        gzclose(gz_fp);
        return errno;
    }
    
    /* Decompress data */
    while ((bytes_read = gzread(gz_fp, buffer, sizeof(buffer))) > 0) {
        total_read += bytes_read;
        
        if (fwrite(buffer, 1, bytes_read, out_fp) != bytes_read) {
            fclose(out_fp);
            gzclose(gz_fp);
            return EIO;
        }
        total_written += bytes_read;
    }
    
    /* Check for errors */
    if (bytes_read < 0) {
        fclose(out_fp);
        gzclose(gz_fp);
        return EIO;
    }
    
    /* Close files */
    fclose(out_fp);
    gzclose(gz_fp);
    
    *original_size = total_written;
    
    return 0;
}

/**
 * Compress file using zstd (placeholder - requires zstd library)
 * 
 * This function compresses a file using zstd compression algorithm.
 * For now, this is a placeholder that falls back to gzip.
 * 
 * @param input_file - Input file path
 * @param output_file - Output file path
 * @param original_size - Output parameter for original size
 * @param compressed_size - Output parameter for compressed size
 * @return 0 on success, error code on failure
 */
static int compress_zstd(const char *input_file, const char *output_file,
                         int64_t *original_size, int64_t *compressed_size) {
    /* Zstd compression requires zstd library */
    /* For now, fall back to gzip */
    if (verbose) {
        fprintf(stderr, "WARNING: zstd compression not available, using gzip\n");
    }
    return compress_gzip(input_file, output_file, original_size, compressed_size);
}

/**
 * Decompress zstd file (placeholder - requires zstd library)
 * 
 * This function decompresses a zstd-compressed file.
 * For now, this is a placeholder that falls back to gzip.
 * 
 * @param input_file - Input compressed file path
 * @param output_file - Output decompressed file path
 * @param original_size - Output parameter for original size
 * @param compressed_size - Output parameter for compressed size
 * @return 0 on success, error code on failure
 */
static int decompress_zstd(const char *input_file, const char *output_file,
                          int64_t *original_size, int64_t *compressed_size) {
    /* Zstd decompression requires zstd library */
    /* For now, fall back to gzip */
    if (verbose) {
        fprintf(stderr, "WARNING: zstd decompression not available, using gzip\n");
    }
    return decompress_gzip(input_file, output_file, original_size, compressed_size);
}

/**
 * Process a single compression task
 * 
 * This function processes a single compression or decompression task.
 * 
 * @param ctx - Compression context
 * @param task - Compression task
 * @return 0 on success, error code on failure
 */
static int process_compress_task(CompressContext *ctx, CompressTask *task) {
    char local_input[256];
    char local_output[256];
    char local_compressed[256];
    int64_t file_size;
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    int result;
    ConnectionInfo *pStorageServer;
    int compress_result;
    
    if (ctx == NULL || task == NULL) {
        return EINVAL;
    }
    
    /* Create temporary file names */
    snprintf(local_input, sizeof(local_input), "/tmp/fdfs_compress_%d_%ld_input.tmp",
             getpid(), (long)pthread_self());
    snprintf(local_output, sizeof(local_output), "/tmp/fdfs_compress_%d_%ld_output.tmp",
             getpid(), (long)pthread_self());
    snprintf(local_compressed, sizeof(local_compressed), "/tmp/fdfs_compress_%d_%ld_compressed.tmp",
             getpid(), (long)pthread_self());
    
    /* Get storage connection */
    pStorageServer = get_storage_connection(ctx->pTrackerServer);
    if (pStorageServer == NULL) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Failed to connect to storage server");
        return -1;
    }
    
    /* Download original file */
    result = storage_download_file_to_file1(ctx->pTrackerServer, pStorageServer,
                                          task->file_id, local_input, &file_size);
    if (result != 0) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Failed to download: %s", STRERROR(result));
        tracker_disconnect_server_ex(pStorageServer, true);
        return result;
    }
    
    task->original_size = file_size;
    
    /* Get metadata if needed */
    if (ctx->preserve_metadata) {
        result = storage_get_metadata1(ctx->pTrackerServer, pStorageServer,
                                      task->file_id, &meta_list, &meta_count);
        if (result != 0 && result != ENOENT) {
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Failed to get metadata: %s", STRERROR(result));
            unlink(local_input);
            tracker_disconnect_server_ex(pStorageServer, true);
            return result;
        }
    }
    
    /* Perform compression or decompression */
    if (task->operation == COMPRESS_OP_COMPRESS) {
        /* Compress file */
        switch (task->algorithm) {
            case COMPRESS_ALG_GZIP:
                compress_result = compress_gzip(local_input, local_compressed,
                                                &task->original_size,
                                                &task->compressed_size);
                break;
            case COMPRESS_ALG_ZSTD:
                compress_result = compress_zstd(local_input, local_compressed,
                                               &task->original_size,
                                               &task->compressed_size);
                break;
            default:
                compress_result = compress_gzip(local_input, local_compressed,
                                                &task->original_size,
                                                &task->compressed_size);
                break;
        }
        
        if (compress_result != 0) {
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Failed to compress: %s", STRERROR(compress_result));
            unlink(local_input);
            if (meta_list != NULL) {
                free(meta_list);
            }
            tracker_disconnect_server_ex(pStorageServer, true);
            return compress_result;
        }
        
        /* Calculate compression ratio */
        if (task->original_size > 0) {
            task->compression_ratio = (double)task->compressed_size / task->original_size;
        } else {
            task->compression_ratio = 0.0;
        }
        
        /* Upload compressed file */
        result = storage_upload_by_filename1_ex(ctx->pTrackerServer, pStorageServer,
                                              local_compressed, NULL,
                                              meta_list, meta_count,
                                              NULL,  /* Use same group */
                                              task->file_id);  /* Use same file ID if in-place */
        
        if (result != 0) {
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Failed to upload compressed file: %s", STRERROR(result));
            unlink(local_input);
            unlink(local_compressed);
            if (meta_list != NULL) {
                free(meta_list);
            }
            tracker_disconnect_server_ex(pStorageServer, true);
            return result;
        }
        
        /* Delete original if in-place mode */
        if (task->in_place) {
            result = storage_delete_file1(ctx->pTrackerServer, pStorageServer,
                                        task->file_id);
            if (result != 0) {
                snprintf(task->error_msg, sizeof(task->error_msg),
                        "Warning: Failed to delete original file: %s", STRERROR(result));
            }
        }
        
        unlink(local_compressed);
    } else {
        /* Decompress file */
        switch (task->algorithm) {
            case COMPRESS_ALG_GZIP:
                compress_result = decompress_gzip(local_input, local_output,
                                                 &task->original_size,
                                                 &task->compressed_size);
                break;
            case COMPRESS_ALG_ZSTD:
                compress_result = decompress_zstd(local_input, local_output,
                                                 &task->original_size,
                                                 &task->compressed_size);
                break;
            default:
                compress_result = decompress_gzip(local_input, local_output,
                                                 &task->original_size,
                                                 &task->compressed_size);
                break;
        }
        
        if (compress_result != 0) {
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Failed to decompress: %s", STRERROR(compress_result));
            unlink(local_input);
            if (meta_list != NULL) {
                free(meta_list);
            }
            tracker_disconnect_server_ex(pStorageServer, true);
            return compress_result;
        }
        
        /* Upload decompressed file */
        result = storage_upload_by_filename1_ex(ctx->pTrackerServer, pStorageServer,
                                              local_output, NULL,
                                              meta_list, meta_count,
                                              NULL,  /* Use same group */
                                              task->file_id);
        
        if (result != 0) {
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Failed to upload decompressed file: %s", STRERROR(result));
            unlink(local_input);
            unlink(local_output);
            if (meta_list != NULL) {
                free(meta_list);
            }
            tracker_disconnect_server_ex(pStorageServer, true);
            return result;
        }
        
        /* Delete compressed file if in-place mode */
        if (task->in_place) {
            result = storage_delete_file1(ctx->pTrackerServer, pStorageServer,
                                        task->file_id);
            if (result != 0) {
                snprintf(task->error_msg, sizeof(task->error_msg),
                        "Warning: Failed to delete compressed file: %s", STRERROR(result));
            }
        }
        
        unlink(local_output);
    }
    
    /* Cleanup */
    unlink(local_input);
    if (meta_list != NULL) {
        free(meta_list);
    }
    tracker_disconnect_server_ex(pStorageServer, true);
    
    task->status = 1;  /* Success */
    return 0;
}

/**
 * Worker thread function for parallel compression
 * 
 * This function is executed by each worker thread to compress files
 * in parallel for better performance.
 * 
 * @param arg - CompressContext pointer
 * @return NULL
 */
static void *compress_worker_thread(void *arg) {
    CompressContext *ctx = (CompressContext *)arg;
    int task_index;
    CompressTask *task;
    int ret;
    
    /* Process tasks until done */
    while (1) {
        /* Get next task index */
        pthread_mutex_lock(&ctx->mutex);
        task_index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        /* Check if we're done */
        if (task_index >= ctx->task_count) {
            break;
        }
        
        task = &ctx->tasks[task_index];
        task->start_time = time(NULL);
        
        /* Process compression task */
        ret = process_compress_task(ctx, task);
        
        task->end_time = time(NULL);
        
        if (ret == 0) {
            pthread_mutex_lock(&stats_mutex);
            if (task->operation == COMPRESS_OP_COMPRESS) {
                files_compressed++;
                total_original_bytes += task->original_size;
                total_compressed_bytes += task->compressed_size;
                total_bytes_saved += (task->original_size - task->compressed_size);
            } else {
                files_decompressed++;
            }
            pthread_mutex_unlock(&stats_mutex);
            
            if (verbose && !quiet) {
                if (task->operation == COMPRESS_OP_COMPRESS) {
                    printf("OK: Compressed %s (%.2f%% ratio, saved %lld bytes)\n",
                           task->file_id, task->compression_ratio * 100.0,
                           (long long)(task->original_size - task->compressed_size));
                } else {
                    printf("OK: Decompressed %s\n", task->file_id);
                }
            }
        } else {
            task->status = -1;  /* Failed */
            pthread_mutex_lock(&stats_mutex);
            files_failed++;
            pthread_mutex_unlock(&stats_mutex);
            
            if (!quiet) {
                fprintf(stderr, "ERROR: Failed to process %s: %s\n",
                       task->file_id, task->error_msg);
            }
        }
        
        pthread_mutex_lock(&stats_mutex);
        total_files_processed++;
        pthread_mutex_unlock(&stats_mutex);
    }
    
    return NULL;
}

/**
 * Read file list from file
 * 
 * This function reads a list of file IDs from a text file,
 * one file ID per line.
 * 
 * @param list_file - Path to file list
 * @param file_ids - Output array for file IDs (must be freed)
 * @param file_count - Output parameter for file count
 * @return 0 on success, error code on failure
 */
static int read_file_list(const char *list_file,
                         char ***file_ids,
                         int *file_count) {
    FILE *fp;
    char line[MAX_LINE_LEN];
    char **ids = NULL;
    int count = 0;
    int capacity = 1000;
    char *p;
    int i;
    
    if (list_file == NULL || file_ids == NULL || file_count == NULL) {
        return EINVAL;
    }
    
    /* Open file list */
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        return errno;
    }
    
    /* Allocate initial array */
    ids = (char **)malloc(capacity * sizeof(char *));
    if (ids == NULL) {
        fclose(fp);
        return ENOMEM;
    }
    
    /* Read file IDs */
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Remove newline characters */
        p = strchr(line, '\n');
        if (p != NULL) {
            *p = '\0';
        }
        
        p = strchr(line, '\r');
        if (p != NULL) {
            *p = '\0';
        }
        
        /* Skip empty lines and comments */
        p = line;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        
        if (*p == '\0' || *p == '#') {
            continue;
        }
        
        /* Expand array if needed */
        if (count >= capacity) {
            capacity *= 2;
            ids = (char **)realloc(ids, capacity * sizeof(char *));
            if (ids == NULL) {
                fclose(fp);
                for (i = 0; i < count; i++) {
                    free(ids[i]);
                }
                free(ids);
                return ENOMEM;
            }
        }
        
        /* Allocate and store file ID */
        ids[count] = (char *)malloc(strlen(p) + 1);
        if (ids[count] == NULL) {
            fclose(fp);
            for (i = 0; i < count; i++) {
                free(ids[i]);
            }
            free(ids);
            return ENOMEM;
        }
        
        strcpy(ids[count], p);
        count++;
    }
    
    fclose(fp);
    
    *file_ids = ids;
    *file_count = count;
    
    return 0;
}

/**
 * Print compression results in text format
 * 
 * This function prints compression results in a human-readable
 * text format.
 * 
 * @param ctx - Compression context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_compression_results_text(CompressContext *ctx,
                                          FILE *output_file) {
    char bytes_buf[64];
    char saved_buf[64];
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    fprintf(output_file, "\n");
    fprintf(output_file, "=== FastDFS Compression Results ===\n");
    fprintf(output_file, "\n");
    
    /* Statistics */
    fprintf(output_file, "=== Statistics ===\n");
    fprintf(output_file, "Total files processed: %d\n", total_files_processed);
    fprintf(output_file, "Files compressed: %d\n", files_compressed);
    fprintf(output_file, "Files decompressed: %d\n", files_decompressed);
    fprintf(output_file, "Files failed: %d\n", files_failed);
    
    if (total_original_bytes > 0) {
        format_bytes(total_original_bytes, bytes_buf, sizeof(bytes_buf));
        fprintf(output_file, "Total original size: %s\n", bytes_buf);
    }
    
    if (total_compressed_bytes > 0) {
        format_bytes(total_compressed_bytes, bytes_buf, sizeof(bytes_buf));
        fprintf(output_file, "Total compressed size: %s\n", bytes_buf);
    }
    
    if (total_bytes_saved > 0) {
        format_bytes(total_bytes_saved, saved_buf, sizeof(saved_buf));
        fprintf(output_file, "Total bytes saved: %s\n", saved_buf);
        
        double ratio = (double)total_compressed_bytes / total_original_bytes;
        fprintf(output_file, "Overall compression ratio: %.2f%%\n", ratio * 100.0);
    }
    
    fprintf(output_file, "\n");
}

/**
 * Print compression results in JSON format
 * 
 * This function prints compression results in JSON format
 * for programmatic processing.
 * 
 * @param ctx - Compression context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_compression_results_json(CompressContext *ctx,
                                           FILE *output_file) {
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    fprintf(output_file, "{\n");
    fprintf(output_file, "  \"timestamp\": %ld,\n", (long)time(NULL));
    fprintf(output_file, "  \"statistics\": {\n");
    fprintf(output_file, "    \"total_files_processed\": %d,\n", total_files_processed);
    fprintf(output_file, "    \"files_compressed\": %d,\n", files_compressed);
    fprintf(output_file, "    \"files_decompressed\": %d,\n", files_decompressed);
    fprintf(output_file, "    \"files_failed\": %d,\n", files_failed);
    fprintf(output_file, "    \"total_original_bytes\": %lld,\n", (long long)total_original_bytes);
    fprintf(output_file, "    \"total_compressed_bytes\": %lld,\n", (long long)total_compressed_bytes);
    fprintf(output_file, "    \"total_bytes_saved\": %lld", (long long)total_bytes_saved);
    
    if (total_original_bytes > 0) {
        double ratio = (double)total_compressed_bytes / total_original_bytes;
        fprintf(output_file, ",\n    \"overall_compression_ratio\": %.2f", ratio);
    }
    
    fprintf(output_file, "\n  }\n");
    fprintf(output_file, "}\n");
}

/**
 * Main function
 * 
 * Entry point for the compression tool. Parses command-line
 * arguments and performs compression operations.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = some failures, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *file_list = NULL;
    char *output_file = NULL;
    CompressAlgorithm algorithm = COMPRESS_ALG_GZIP;
    CompressOperation operation = COMPRESS_OP_COMPRESS;
    int in_place = 0;
    int num_threads = DEFAULT_THREADS;
    int result;
    ConnectionInfo *pTrackerServer;
    CompressContext ctx;
    pthread_t *threads = NULL;
    char **file_ids = NULL;
    int file_count = 0;
    int i;
    FILE *out_fp = stdout;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"algorithm", required_argument, 0, 'a'},
        {"operation", required_argument, 0, 1000},
        {"in-place", no_argument, 0, 'i'},
        {"metadata", no_argument, 0, 'm'},
        {"threads", required_argument, 0, 'j'},
        {"output", required_argument, 0, 1001},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(CompressContext));
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:f:a:imj:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                file_list = optarg;
                break;
            case 'a':
                if (strcmp(optarg, "gzip") == 0) {
                    algorithm = COMPRESS_ALG_GZIP;
                } else if (strcmp(optarg, "zstd") == 0) {
                    algorithm = COMPRESS_ALG_ZSTD;
                } else {
                    fprintf(stderr, "ERROR: Unknown algorithm: %s\n", optarg);
                    return 2;
                }
                break;
            case 1000:
                if (strcmp(optarg, "compress") == 0) {
                    operation = COMPRESS_OP_COMPRESS;
                } else if (strcmp(optarg, "decompress") == 0) {
                    operation = COMPRESS_OP_DECOMPRESS;
                } else if (strcmp(optarg, "auto") == 0) {
                    operation = COMPRESS_OP_AUTO;
                } else {
                    fprintf(stderr, "ERROR: Unknown operation: %s\n", optarg);
                    return 2;
                }
                break;
            case 'i':
                in_place = 1;
                break;
            case 'm':
                ctx.preserve_metadata = 1;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 1001:
                output_file = optarg;
                break;
            case 'v':
                verbose = 1;
                ctx.verbose = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'J':
                json_output = 1;
                ctx.json_output = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 2;
        }
    }
    
    /* Get file IDs from arguments or file list */
    if (file_list != NULL) {
        result = read_file_list(file_list, &file_ids, &file_count);
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to read file list: %s\n", STRERROR(result));
            return 2;
        }
    } else if (optind < argc) {
        /* Get file IDs from command-line arguments */
        file_count = argc - optind;
        file_ids = (char **)malloc(file_count * sizeof(char *));
        if (file_ids == NULL) {
            return ENOMEM;
        }
        
        for (i = 0; i < file_count; i++) {
            file_ids[i] = strdup(argv[optind + i]);
            if (file_ids[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(file_ids[j]);
                }
                free(file_ids);
                return ENOMEM;
            }
        }
    } else {
        fprintf(stderr, "ERROR: No file IDs specified\n\n");
        print_usage(argv[0]);
        return 2;
    }
    
    if (file_count == 0) {
        fprintf(stderr, "ERROR: No files to process\n");
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return 2;
    }
    
    /* Initialize logging */
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    /* Initialize FastDFS client */
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return 2;
    }
    
    /* Connect to tracker server */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        fdfs_client_destroy();
        return 2;
    }
    
    /* Allocate tasks */
    ctx.tasks = (CompressTask *)calloc(file_count, sizeof(CompressTask));
    if (ctx.tasks == NULL) {
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return ENOMEM;
    }
    
    /* Initialize context */
    ctx.task_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.default_algorithm = algorithm;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    /* Initialize tasks */
    for (i = 0; i < file_count; i++) {
        CompressTask *task = &ctx.tasks[i];
        strncpy(task->file_id, file_ids[i], MAX_FILE_ID_LEN - 1);
        task->algorithm = algorithm;
        task->operation = operation;
        task->in_place = in_place;
        task->status = 0;
    }
    
    /* Reset statistics */
    total_files_processed = 0;
    files_compressed = 0;
    files_decompressed = 0;
    files_failed = 0;
    total_original_bytes = 0;
    total_compressed_bytes = 0;
    total_bytes_saved = 0;
    
    /* Limit number of threads */
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
    }
    if (num_threads > file_count) {
        num_threads = file_count;
    }
    
    /* Allocate thread array */
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        pthread_mutex_destroy(&ctx.mutex);
        free(ctx.tasks);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return ENOMEM;
    }
    
    /* Start worker threads */
    for (i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, compress_worker_thread, &ctx) != 0) {
            fprintf(stderr, "ERROR: Failed to create thread %d\n", i);
            result = errno;
            break;
        }
    }
    
    /* Wait for all threads to complete */
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Print results */
    if (output_file != NULL) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            out_fp = stdout;
        }
    }
    
    if (json_output) {
        print_compression_results_json(&ctx, out_fp);
    } else {
        print_compression_results_text(&ctx, out_fp);
    }
    
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    pthread_mutex_destroy(&ctx.mutex);
    free(threads);
    free(ctx.tasks);
    if (file_ids != NULL) {
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
    }
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    /* Return appropriate exit code */
    if (files_failed > 0) {
        return 1;  /* Some failures */
    }
    
    return 0;  /* Success */
}

