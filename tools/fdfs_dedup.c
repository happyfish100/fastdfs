/**
 * FastDFS Deduplication Tool
 * 
 * Identifies duplicate files based on CRC32 checksums
 * Helps optimize storage by finding redundant files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"
#include "fastcommon/hash.h"

#define MAX_FILE_ID_LEN 256
#define HASH_TABLE_SIZE 100000
#define MAX_THREADS 10

typedef struct FileNode {
    char file_id[MAX_FILE_ID_LEN];
    int64_t file_size;
    uint32_t crc32;
    time_t create_time;
    struct FileNode *next;
} FileNode;

typedef struct {
    FileNode *buckets[HASH_TABLE_SIZE];
    pthread_mutex_t locks[HASH_TABLE_SIZE];
} HashTable;

typedef struct {
    char *file_ids;
    int file_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    HashTable *hash_table;
    int verbose;
} ScanContext;

static int total_files = 0;
static int scanned_files = 0;
static int duplicate_files = 0;
static int64_t total_bytes = 0;
static int64_t duplicate_bytes = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -f <file_list>\n", program_name);
    printf("\n");
    printf("Find duplicate files in FastDFS based on CRC32 checksums\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST      File list to scan (one file ID per line)\n");
    printf("  -o, --output FILE    Output duplicate report (default: stdout)\n");
    printf("  -j, --threads NUM    Number of parallel threads (default: 4, max: 10)\n");
    printf("  -s, --min-size SIZE  Minimum file size in bytes (default: 0)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f all_files.txt\n", program_name);
    printf("  %s -f files.txt -o duplicates.txt -j 8\n", program_name);
    printf("  %s -f files.txt -s 1048576  # Min 1MB\n", program_name);
}

static unsigned int hash_crc32(uint32_t crc32, int64_t size) {
    unsigned long long combined = ((unsigned long long)crc32 << 32) | (size & 0xFFFFFFFF);
    return (unsigned int)(combined % HASH_TABLE_SIZE);
}

static HashTable *create_hash_table(void) {
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    if (table == NULL) {
        return NULL;
    }
    
    memset(table->buckets, 0, sizeof(table->buckets));
    
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        pthread_mutex_init(&table->locks[i], NULL);
    }
    
    return table;
}

static void free_hash_table(HashTable *table) {
    if (table == NULL) {
        return;
    }
    
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        FileNode *node = table->buckets[i];
        while (node != NULL) {
            FileNode *next = node->next;
            free(node);
            node = next;
        }
        pthread_mutex_destroy(&table->locks[i]);
    }
    
    free(table);
}

static int add_file_to_table(HashTable *table, const char *file_id,
                             int64_t size, uint32_t crc32, time_t create_time) {
    unsigned int bucket = hash_crc32(crc32, size);
    int is_duplicate = 0;
    
    pthread_mutex_lock(&table->locks[bucket]);
    
    FileNode *node = table->buckets[bucket];
    while (node != NULL) {
        if (node->crc32 == crc32 && node->file_size == size) {
            is_duplicate = 1;
            
            pthread_mutex_lock(&stats_mutex);
            duplicate_files++;
            duplicate_bytes += size;
            pthread_mutex_unlock(&stats_mutex);
            
            break;
        }
        node = node->next;
    }
    
    FileNode *new_node = (FileNode *)malloc(sizeof(FileNode));
    if (new_node != NULL) {
        strncpy(new_node->file_id, file_id, MAX_FILE_ID_LEN - 1);
        new_node->file_size = size;
        new_node->crc32 = crc32;
        new_node->create_time = create_time;
        new_node->next = table->buckets[bucket];
        table->buckets[bucket] = new_node;
    }
    
    pthread_mutex_unlock(&table->locks[bucket]);
    
    return is_duplicate;
}

static int scan_file(ConnectionInfo *pTrackerServer, const char *file_id,
                    HashTable *table, int verbose) {
    FDFSFileInfo file_info;
    int result;
    ConnectionInfo *pStorageServer;
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        if (verbose) {
            fprintf(stderr, "ERROR: Failed to connect to storage server for %s\n", file_id);
        }
        return -1;
    }
    
    result = storage_query_file_info1(pTrackerServer, pStorageServer, file_id, &file_info);
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    if (result != 0) {
        if (verbose) {
            fprintf(stderr, "ERROR: Failed to query %s: %s\n", file_id, STRERROR(result));
        }
        return result;
    }
    
    int is_dup = add_file_to_table(table, file_id, file_info.file_size,
                                   file_info.crc32, file_info.create_timestamp);
    
    pthread_mutex_lock(&stats_mutex);
    scanned_files++;
    total_bytes += file_info.file_size;
    pthread_mutex_unlock(&stats_mutex);
    
    if (verbose && is_dup) {
        printf("DUPLICATE: %s (size: %lld, CRC32: %08X)\n",
               file_id, (long long)file_info.file_size, file_info.crc32);
    }
    
    return 0;
}

static void *scan_worker(void *arg) {
    ScanContext *ctx = (ScanContext *)arg;
    int index;
    char file_id[MAX_FILE_ID_LEN];
    
    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->current_index >= ctx->file_count) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        strncpy(file_id, ctx->file_ids + index * MAX_FILE_ID_LEN, MAX_FILE_ID_LEN - 1);
        file_id[MAX_FILE_ID_LEN - 1] = '\0';
        
        scan_file(ctx->pTrackerServer, file_id, ctx->hash_table, ctx->verbose);
        
        if (!ctx->verbose && scanned_files % 100 == 0) {
            printf("\rScanned: %d/%d files...", scanned_files, total_files);
            fflush(stdout);
        }
    }
    
    return NULL;
}

static int load_file_list(const char *list_file, char **file_ids, int *count) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    int capacity = 10000;
    int file_count = 0;
    char *id_array;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    id_array = (char *)malloc(capacity * MAX_FILE_ID_LEN);
    if (id_array == NULL) {
        fclose(fp);
        return ENOMEM;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = strchr(line, '\n');
        if (p != NULL) {
            *p = '\0';
        }
        
        p = strchr(line, '\r');
        if (p != NULL) {
            *p = '\0';
        }
        
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        if (file_count >= capacity) {
            capacity *= 2;
            id_array = (char *)realloc(id_array, capacity * MAX_FILE_ID_LEN);
            if (id_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        strncpy(id_array + file_count * MAX_FILE_ID_LEN, line, MAX_FILE_ID_LEN - 1);
        file_count++;
    }
    
    fclose(fp);
    
    *file_ids = id_array;
    *count = file_count;
    total_files = file_count;
    
    return 0;
}

static void generate_duplicate_report(HashTable *table, FILE *output, int min_size) {
    int duplicate_groups = 0;
    int64_t potential_savings = 0;
    
    fprintf(output, "\n");
    fprintf(output, "=== FastDFS Duplicate File Report ===\n");
    fprintf(output, "\n");
    
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        FileNode *node = table->buckets[i];
        
        if (node == NULL || node->next == NULL) {
            continue;
        }
        
        int group_count = 0;
        int64_t group_size = 0;
        FileNode *temp = node;
        
        while (temp != NULL) {
            if (temp->file_size >= min_size) {
                group_count++;
                group_size = temp->file_size;
            }
            temp = temp->next;
        }
        
        if (group_count > 1) {
            duplicate_groups++;
            int64_t savings = group_size * (group_count - 1);
            potential_savings += savings;
            
            fprintf(output, "Duplicate Group #%d:\n", duplicate_groups);
            fprintf(output, "  Size: %lld bytes (%.2f MB)\n",
                   (long long)group_size, group_size / (1024.0 * 1024.0));
            fprintf(output, "  CRC32: %08X\n", node->crc32);
            fprintf(output, "  Count: %d files\n", group_count);
            fprintf(output, "  Potential savings: %lld bytes (%.2f MB)\n",
                   (long long)savings, savings / (1024.0 * 1024.0));
            fprintf(output, "  Files:\n");
            
            temp = node;
            while (temp != NULL) {
                if (temp->file_size >= min_size) {
                    char time_str[64];
                    struct tm *tm_info = localtime(&temp->create_time);
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
                    fprintf(output, "    - %s (created: %s)\n", temp->file_id, time_str);
                }
                temp = temp->next;
            }
            fprintf(output, "\n");
        }
    }
    
    fprintf(output, "=== Summary ===\n");
    fprintf(output, "Total files scanned: %d\n", scanned_files);
    fprintf(output, "Total size: %lld bytes (%.2f GB)\n",
           (long long)total_bytes, total_bytes / (1024.0 * 1024.0 * 1024.0));
    fprintf(output, "Duplicate files: %d\n", duplicate_files);
    fprintf(output, "Duplicate size: %lld bytes (%.2f GB)\n",
           (long long)duplicate_bytes, duplicate_bytes / (1024.0 * 1024.0 * 1024.0));
    fprintf(output, "Duplicate groups: %d\n", duplicate_groups);
    fprintf(output, "Potential storage savings: %lld bytes (%.2f GB)\n",
           (long long)potential_savings, potential_savings / (1024.0 * 1024.0 * 1024.0));
    
    if (total_bytes > 0) {
        double dup_percent = (duplicate_bytes * 100.0) / total_bytes;
        fprintf(output, "Duplication rate: %.2f%%\n", dup_percent);
    }
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    char *output_file = NULL;
    int num_threads = 4;
    int64_t min_size = 0;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    char *file_ids = NULL;
    int file_count = 0;
    HashTable *hash_table;
    ScanContext ctx;
    pthread_t *threads;
    FILE *output;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"output", required_argument, 0, 'o'},
        {"threads", required_argument, 0, 'j'},
        {"min-size", required_argument, 0, 's'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:f:o:j:s:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 's':
                min_size = atoll(optarg);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (list_file == NULL) {
        fprintf(stderr, "ERROR: File list required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    result = load_file_list(list_file, &file_ids, &file_count);
    if (result != 0) {
        return result;
    }
    
    if (file_count == 0) {
        printf("No files to scan\n");
        free(file_ids);
        return 0;
    }
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        free(file_ids);
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        free(file_ids);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    hash_table = create_hash_table();
    if (hash_table == NULL) {
        fprintf(stderr, "ERROR: Failed to create hash table\n");
        free(file_ids);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return ENOMEM;
    }
    
    printf("Scanning %d files for duplicates using %d threads...\n", file_count, num_threads);
    if (min_size > 0) {
        printf("Minimum file size: %lld bytes\n", (long long)min_size);
    }
    printf("\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.file_ids = file_ids;
    ctx.file_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.hash_table = hash_table;
    ctx.verbose = verbose;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, scan_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000LL +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
    
    if (!verbose) {
        printf("\n");
    }
    
    if (output_file != NULL) {
        output = fopen(output_file, "w");
        if (output == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            output = stdout;
        }
    } else {
        output = stdout;
    }
    
    generate_duplicate_report(hash_table, output, min_size);
    
    fprintf(output, "\nScan completed in %lld ms (%.2f files/sec)\n",
           elapsed_ms, file_count * 1000.0 / elapsed_ms);
    
    if (output != stdout) {
        fclose(output);
        printf("\nReport saved to: %s\n", output_file);
    }
    
    free(file_ids);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    free_hash_table(hash_table);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return 0;
}
