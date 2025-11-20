/**
 * Test suite for FastDFS truncate operations
 * Tests storage_truncate_file1 function for resizing appender files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fastcommon/logger.h"
#include "fastdfs/fdfs_client.h"
#include "dfs_func.h"

#define INITIAL_FILE_SIZE 1024
#define APPEND_SIZE 512

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static void print_test_result(const char *test_name, int passed) {
    tests_run++;
    if (passed) {
        tests_passed++;
        printf("[PASS] %s\n", test_name);
    } else {
        tests_failed++;
        printf("[FAIL] %s\n", test_name);
    }
}

static int create_test_file(const char *filename, int size) {
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        return -1;
    }
    
    for (int i = 0; i < size; i++) {
        fputc('A' + (i % 26), fp);
    }
    
    fclose(fp);
    return 0;
}

static int64_t get_file_size(ConnectionInfo *pTrackerServer,
                             ConnectionInfo *pStorageServer,
                             const char *file_id) {
    FDFSFileInfo file_info;
    int result = storage_query_file_info_ex1(pTrackerServer, pStorageServer,
                                             file_id, &file_info);
    if (result != 0) {
        return -1;
    }
    return file_info.file_size;
}

/**
 * Test 1: Truncate to smaller size
 */
static void test_truncate_to_smaller(ConnectionInfo *pTrackerServer,
                                    ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    int64_t new_size = 512;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_small_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0) {
        print_test_result("truncate_to_smaller - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_to_smaller - upload", 0);
        unlink(local_file);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer,
                                    file_id, new_size);
    
    int64_t actual_size = get_file_size(pTrackerServer, pStorageServer, file_id);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("truncate_to_smaller", 
                     result == 0 && actual_size == new_size);
}

/**
 * Test 2: Truncate to zero
 */
static void test_truncate_to_zero(ConnectionInfo *pTrackerServer,
                                 ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_zero_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0) {
        print_test_result("truncate_to_zero - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_to_zero - upload", 0);
        unlink(local_file);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer, file_id, 0);
    
    int64_t actual_size = get_file_size(pTrackerServer, pStorageServer, file_id);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("truncate_to_zero", result == 0 && actual_size == 0);
}

/**
 * Test 3: Truncate after append
 */
static void test_truncate_after_append(ConnectionInfo *pTrackerServer,
                                       ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char append_file[256];
    int result;
    int64_t truncate_size = INITIAL_FILE_SIZE + 256;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_append_%d.dat", getpid());
    snprintf(append_file, sizeof(append_file), "/tmp/test_trunc_append_data_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0 ||
        create_test_file(append_file, APPEND_SIZE) != 0) {
        print_test_result("truncate_after_append - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_after_append - upload", 0);
        unlink(local_file);
        unlink(append_file);
        return;
    }
    
    result = storage_append_by_filename1(pTrackerServer, pStorageServer,
                                        append_file, file_id);
    if (result != 0) {
        print_test_result("truncate_after_append - append", 0);
        unlink(local_file);
        unlink(append_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer,
                                    file_id, truncate_size);
    
    int64_t actual_size = get_file_size(pTrackerServer, pStorageServer, file_id);
    
    unlink(local_file);
    unlink(append_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("truncate_after_append",
                     result == 0 && actual_size == truncate_size);
}

/**
 * Test 4: Multiple truncates
 */
static void test_multiple_truncates(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_multi_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0) {
        print_test_result("multiple_truncates - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("multiple_truncates - upload", 0);
        unlink(local_file);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer, file_id, 800);
    if (result != 0) {
        unlink(local_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        print_test_result("multiple_truncates", 0);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer, file_id, 600);
    if (result != 0) {
        unlink(local_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        print_test_result("multiple_truncates", 0);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer, file_id, 400);
    
    int64_t actual_size = get_file_size(pTrackerServer, pStorageServer, file_id);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("multiple_truncates", result == 0 && actual_size == 400);
}

/**
 * Test 5: Truncate to same size
 */
static void test_truncate_same_size(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_same_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0) {
        print_test_result("truncate_same_size - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_same_size - upload", 0);
        unlink(local_file);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer,
                                    file_id, INITIAL_FILE_SIZE);
    
    int64_t actual_size = get_file_size(pTrackerServer, pStorageServer, file_id);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("truncate_same_size",
                     result == 0 && actual_size == INITIAL_FILE_SIZE);
}

/**
 * Test 6: Truncate large file
 */
static void test_truncate_large_file(ConnectionInfo *pTrackerServer,
                                    ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    int large_size = 10 * 1024 * 1024;  // 10MB
    int64_t truncate_size = 5 * 1024 * 1024;  // 5MB
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_large_%d.dat", getpid());
    
    if (create_test_file(local_file, large_size) != 0) {
        print_test_result("truncate_large_file - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_large_file - upload", 0);
        unlink(local_file);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer,
                                    file_id, truncate_size);
    
    int64_t actual_size = get_file_size(pTrackerServer, pStorageServer, file_id);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("truncate_large_file",
                     result == 0 && actual_size == truncate_size);
}

/**
 * Test 7: Error - truncate non-appender file
 */
static void test_truncate_non_appender(ConnectionInfo *pTrackerServer,
                                      ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_noappend_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0) {
        print_test_result("truncate_non_appender - file creation", 0);
        return;
    }
    
    // Upload as regular file (not appender)
    result = upload_file(pTrackerServer, pStorageServer, local_file,
                        file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_non_appender - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Try to truncate (should fail)
    result = storage_truncate_file1(pTrackerServer, pStorageServer, file_id, 512);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    // Should fail
    print_test_result("truncate_non_appender", result != 0);
}

/**
 * Test 8: Error - invalid file ID
 */
static void test_truncate_invalid_file(ConnectionInfo *pTrackerServer,
                                      ConnectionInfo *pStorageServer) {
    int result = storage_truncate_file1(pTrackerServer, pStorageServer,
                                       "group1/M00/00/00/invalid_file", 512);
    
    print_test_result("truncate_invalid_file", result != 0);
}

/**
 * Test 9: Error - negative size
 */
static void test_truncate_negative_size(ConnectionInfo *pTrackerServer,
                                       ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_neg_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0) {
        print_test_result("truncate_negative_size - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_negative_size - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Try negative size (should fail)
    result = storage_truncate_file1(pTrackerServer, pStorageServer, file_id, -100);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("truncate_negative_size", result != 0);
}

/**
 * Test 10: Truncate then append
 */
static void test_truncate_then_append(ConnectionInfo *pTrackerServer,
                                     ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char append_file[256];
    int result;
    int64_t truncate_size = 512;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_trunc_then_app_%d.dat", getpid());
    snprintf(append_file, sizeof(append_file), "/tmp/test_trunc_then_app_data_%d.dat", getpid());
    
    if (create_test_file(local_file, INITIAL_FILE_SIZE) != 0 ||
        create_test_file(append_file, APPEND_SIZE) != 0) {
        print_test_result("truncate_then_append - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("truncate_then_append - upload", 0);
        unlink(local_file);
        unlink(append_file);
        return;
    }
    
    result = storage_truncate_file1(pTrackerServer, pStorageServer,
                                    file_id, truncate_size);
    if (result != 0) {
        print_test_result("truncate_then_append - truncate", 0);
        unlink(local_file);
        unlink(append_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        return;
    }
    
    result = storage_append_by_filename1(pTrackerServer, pStorageServer,
                                        append_file, file_id);
    
    int64_t actual_size = get_file_size(pTrackerServer, pStorageServer, file_id);
    int64_t expected_size = truncate_size + APPEND_SIZE;
    
    unlink(local_file);
    unlink(append_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("truncate_then_append",
                     result == 0 && actual_size == expected_size);
}

int main(int argc, char *argv[]) {
    char *conf_filename;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    int result;
    
    printf("=== FastDFS Truncate Operations Test Suite ===\n\n");
    
    if (argc < 2) {
        conf_filename = "/etc/fdfs/client.conf";
    } else {
        conf_filename = argv[1];
    }
    
    log_init();
    g_log_context.log_level = LOG_ERR;
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        printf("ERROR: Failed to initialize FastDFS client\n");
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("ERROR: Failed to connect to tracker server\n");
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        printf("ERROR: Failed to connect to storage server\n");
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    printf("Running truncate operation tests...\n\n");
    
    test_truncate_to_smaller(pTrackerServer, pStorageServer);
    test_truncate_to_zero(pTrackerServer, pStorageServer);
    test_truncate_after_append(pTrackerServer, pStorageServer);
    test_multiple_truncates(pTrackerServer, pStorageServer);
    test_truncate_same_size(pTrackerServer, pStorageServer);
    test_truncate_large_file(pTrackerServer, pStorageServer);
    test_truncate_non_appender(pTrackerServer, pStorageServer);
    test_truncate_invalid_file(pTrackerServer, pStorageServer);
    test_truncate_negative_size(pTrackerServer, pStorageServer);
    test_truncate_then_append(pTrackerServer, pStorageServer);
    
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    tracker_disconnect_server_ex(pStorageServer, true);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return tests_failed > 0 ? 1 : 0;
}
