/**
 * Test suite for FastDFS file info and query operations
 * Tests storage_query_file_info and storage_file_exist functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"

#define TEST_FILE_SIZE 2048

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

/**
 * Test 1: Query file info for existing file
 */
static void test_query_existing_file(ConnectionInfo *pTrackerServer,
                                     ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    FDFSFileInfo file_info;
    int result;
    struct stat st;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_fileinfo_%d.dat", getpid());
    
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("query_existing_file - file creation", 0);
        return;
    }
    
    stat(local_file, &st);
    
    result = upload_file(pTrackerServer, pStorageServer, local_file,
                        file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("query_existing_file - upload", 0);
        unlink(local_file);
        return;
    }
    
    memset(&file_info, 0, sizeof(file_info));
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                      file_id, &file_info);
    
    int passed = (result == 0 && 
                  file_info.file_size == TEST_FILE_SIZE &&
                  file_info.create_timestamp > 0 &&
                  file_info.crc32 != 0);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("query_existing_file", passed);
}

/**
 * Test 2: Query file info for non-existent file
 */
static void test_query_nonexistent_file(ConnectionInfo *pTrackerServer,
                                       ConnectionInfo *pStorageServer) {
    FDFSFileInfo file_info;
    int result;
    
    memset(&file_info, 0, sizeof(file_info));
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                      "group1/M00/00/00/nonexistent_file.dat",
                                      &file_info);
    
    print_test_result("query_nonexistent_file", result != 0);
}

/**
 * Test 3: Check file existence for existing file
 */
static void test_file_exist_true(ConnectionInfo *pTrackerServer,
                                ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    int exists;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_exist_true_%d.dat", getpid());
    
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("file_exist_true - file creation", 0);
        return;
    }
    
    result = upload_file(pTrackerServer, pStorageServer, local_file,
                        file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("file_exist_true - upload", 0);
        unlink(local_file);
        return;
    }
    
    result = storage_file_exist1(pTrackerServer, pStorageServer, file_id, &exists);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("file_exist_true", result == 0 && exists == 1);
}

/**
 * Test 4: Check file existence for non-existent file
 */
static void test_file_exist_false(ConnectionInfo *pTrackerServer,
                                 ConnectionInfo *pStorageServer) {
    int result;
    int exists = -1;
    
    result = storage_file_exist1(pTrackerServer, pStorageServer,
                                "group1/M00/00/00/nonexistent_file.dat", &exists);
    
    print_test_result("file_exist_false", result == 0 && exists == 0);
}

/**
 * Test 5: Query file info after modification
 */
static void test_query_after_modify(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_file[256];
    FDFSFileInfo file_info_before, file_info_after;
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_query_mod_%d.dat", getpid());
    snprintf(modify_file, sizeof(modify_file), "/tmp/test_query_mod_data_%d.dat", getpid());
    
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0 ||
        create_test_file(modify_file, 100) != 0) {
        print_test_result("query_after_modify - file creation", 0);
        return;
    }
    
    result = upload_appender_file(pTrackerServer, pStorageServer, local_file,
                                  file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("query_after_modify - upload", 0);
        unlink(local_file);
        unlink(modify_file);
        return;
    }
    
    memset(&file_info_before, 0, sizeof(file_info_before));
    storage_query_file_info1(pTrackerServer, pStorageServer, file_id, &file_info_before);
    
    sleep(1);
    
    result = storage_modify_by_filename1(pTrackerServer, pStorageServer,
                                        modify_file, 0, file_id);
    if (result != 0) {
        print_test_result("query_after_modify - modify", 0);
        unlink(local_file);
        unlink(modify_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        return;
    }
    
    memset(&file_info_after, 0, sizeof(file_info_after));
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                      file_id, &file_info_after);
    
    int passed = (result == 0 &&
                  file_info_after.file_size == TEST_FILE_SIZE &&
                  file_info_after.crc32 != file_info_before.crc32);
    
    unlink(local_file);
    unlink(modify_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("query_after_modify", passed);
}

/**
 * Test 6: Query file info for large file
 */
static void test_query_large_file(ConnectionInfo *pTrackerServer,
                                 ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    FDFSFileInfo file_info;
    int result;
    int64_t large_size = 10 * 1024 * 1024;  // 10MB
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_query_large_%d.dat", getpid());
    
    if (create_test_file(local_file, large_size) != 0) {
        print_test_result("query_large_file - file creation", 0);
        return;
    }
    
    result = upload_file(pTrackerServer, pStorageServer, local_file,
                        file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("query_large_file - upload", 0);
        unlink(local_file);
        return;
    }
    
    memset(&file_info, 0, sizeof(file_info));
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                      file_id, &file_info);
    
    int passed = (result == 0 && file_info.file_size == large_size);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("query_large_file", passed);
}

/**
 * Test 7: Query file info with source IP
 */
static void test_query_source_ip(ConnectionInfo *pTrackerServer,
                                ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    FDFSFileInfo file_info;
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_source_ip_%d.dat", getpid());
    
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("query_source_ip - file creation", 0);
        return;
    }
    
    result = upload_file(pTrackerServer, pStorageServer, local_file,
                        file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("query_source_ip - upload", 0);
        unlink(local_file);
        return;
    }
    
    memset(&file_info, 0, sizeof(file_info));
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                      file_id, &file_info);
    
    int passed = (result == 0 && strlen(file_info.source_ip_addr) > 0);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("query_source_ip", passed);
}

/**
 * Test 8: File existence after delete
 */
static void test_exist_after_delete(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    int result;
    int exists_before, exists_after;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_exist_del_%d.dat", getpid());
    
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("exist_after_delete - file creation", 0);
        return;
    }
    
    result = upload_file(pTrackerServer, pStorageServer, local_file,
                        file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("exist_after_delete - upload", 0);
        unlink(local_file);
        return;
    }
    
    storage_file_exist1(pTrackerServer, pStorageServer, file_id, &exists_before);
    
    result = storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    if (result != 0) {
        print_test_result("exist_after_delete - delete", 0);
        unlink(local_file);
        return;
    }
    
    storage_file_exist1(pTrackerServer, pStorageServer, file_id, &exists_after);
    
    unlink(local_file);
    
    print_test_result("exist_after_delete", exists_before == 1 && exists_after == 0);
}

/**
 * Test 9: Query multiple files
 */
static void test_query_multiple_files(ConnectionInfo *pTrackerServer,
                                     ConnectionInfo *pStorageServer) {
    char file_ids[3][128];
    char local_file[256];
    FDFSFileInfo file_infos[3];
    int result;
    int all_passed = 1;
    
    for (int i = 0; i < 3; i++) {
        snprintf(local_file, sizeof(local_file), "/tmp/test_multi_%d_%d.dat", getpid(), i);
        
        if (create_test_file(local_file, TEST_FILE_SIZE * (i + 1)) != 0) {
            all_passed = 0;
            break;
        }
        
        result = upload_file(pTrackerServer, pStorageServer, local_file,
                            file_ids[i], sizeof(file_ids[i]));
        unlink(local_file);
        
        if (result != 0) {
            all_passed = 0;
            break;
        }
    }
    
    if (all_passed) {
        for (int i = 0; i < 3; i++) {
            memset(&file_infos[i], 0, sizeof(FDFSFileInfo));
            result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                             file_ids[i], &file_infos[i]);
            
            if (result != 0 || file_infos[i].file_size != TEST_FILE_SIZE * (i + 1)) {
                all_passed = 0;
                break;
            }
        }
    }
    
    for (int i = 0; i < 3; i++) {
        storage_delete_file1(pTrackerServer, pStorageServer, file_ids[i]);
    }
    
    print_test_result("query_multiple_files", all_passed);
}

/**
 * Test 10: Query file info with invalid file ID format
 */
static void test_query_invalid_format(ConnectionInfo *pTrackerServer,
                                     ConnectionInfo *pStorageServer) {
    FDFSFileInfo file_info;
    int result;
    
    memset(&file_info, 0, sizeof(file_info));
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                      "invalid_format", &file_info);
    
    print_test_result("query_invalid_format", result != 0);
}

/**
 * Test 11: File existence check with empty file ID
 */
static void test_exist_empty_id(ConnectionInfo *pTrackerServer,
                               ConnectionInfo *pStorageServer) {
    int result;
    int exists;
    
    result = storage_file_exist1(pTrackerServer, pStorageServer, "", &exists);
    
    print_test_result("exist_empty_id", result != 0);
}

/**
 * Test 12: Query file info timestamp accuracy
 */
static void test_query_timestamp(ConnectionInfo *pTrackerServer,
                                ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    FDFSFileInfo file_info;
    int result;
    time_t upload_time;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_timestamp_%d.dat", getpid());
    
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("query_timestamp - file creation", 0);
        return;
    }
    
    upload_time = time(NULL);
    
    result = upload_file(pTrackerServer, pStorageServer, local_file,
                        file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("query_timestamp - upload", 0);
        unlink(local_file);
        return;
    }
    
    memset(&file_info, 0, sizeof(file_info));
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                      file_id, &file_info);
    
    int time_diff = abs(file_info.create_timestamp - upload_time);
    int passed = (result == 0 && time_diff <= 2);
    
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("query_timestamp", passed);
}

int main(int argc, char *argv[]) {
    char *conf_filename;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    int result;
    
    printf("=== FastDFS File Info and Query Operations Test Suite ===\n\n");
    
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
    
    printf("Running file info and query tests...\n\n");
    
    test_query_existing_file(pTrackerServer, pStorageServer);
    test_query_nonexistent_file(pTrackerServer, pStorageServer);
    test_file_exist_true(pTrackerServer, pStorageServer);
    test_file_exist_false(pTrackerServer, pStorageServer);
    test_query_after_modify(pTrackerServer, pStorageServer);
    test_query_large_file(pTrackerServer, pStorageServer);
    test_query_source_ip(pTrackerServer, pStorageServer);
    test_exist_after_delete(pTrackerServer, pStorageServer);
    test_query_multiple_files(pTrackerServer, pStorageServer);
    test_query_invalid_format(pTrackerServer, pStorageServer);
    test_exist_empty_id(pTrackerServer, pStorageServer);
    test_query_timestamp(pTrackerServer, pStorageServer);
    
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
