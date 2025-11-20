/**
 * Test suite for FastDFS slave file operations
 * Tests storage_upload_slave_by_filename, storage_upload_slave_by_filebuff
 * 
 * Slave files are variants of master files (e.g., thumbnails, previews)
 * They share the same path but have different prefixes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"

#define MASTER_FILE_SIZE 10240
#define SLAVE_FILE_SIZE 2048

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

static int verify_slave_prefix(const char *master_id, const char *slave_id, 
                               const char *prefix) {
    char master_path[256];
    char slave_path[256];
    char *p;
    
    // Extract paths from file IDs
    strcpy(master_path, strchr(master_id, '/'));
    strcpy(slave_path, strchr(slave_id, '/'));
    
    // Get filename from master
    p = strrchr(master_path, '/');
    if (p == NULL) return 0;
    
    // Check if slave path contains prefix
    return strstr(slave_path, prefix) != NULL;
}

/**
 * Test 1: Upload slave file by filename
 */
static void test_upload_slave_by_filename(ConnectionInfo *pTrackerServer,
                                         ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    int result;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_master_%d.jpg", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_slave_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("upload_slave_by_filename - file creation", 0);
        return;
    }
    
    // Upload master file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("upload_slave_by_filename - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    // Upload slave file
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "_thumb",
                                               "jpg", NULL, 0, slave_id);
    
    int passed = (result == 0 && strlen(slave_id) > 0 &&
                  verify_slave_prefix(master_id, slave_id, "_thumb"));
    
    unlink(master_file);
    unlink(slave_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("upload_slave_by_filename", passed);
}

/**
 * Test 2: Upload slave file by buffer
 */
static void test_upload_slave_by_buffer(ConnectionInfo *pTrackerServer,
                                       ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char *slave_buff;
    int result;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_master_buf_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0) {
        print_test_result("upload_slave_by_buffer - file creation", 0);
        return;
    }
    
    // Upload master file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("upload_slave_by_buffer - master upload", 0);
        unlink(master_file);
        return;
    }
    
    // Create slave buffer
    slave_buff = (char *)malloc(SLAVE_FILE_SIZE);
    memset(slave_buff, 'S', SLAVE_FILE_SIZE);
    
    // Upload slave from buffer
    result = storage_upload_slave_by_filebuff1(pTrackerServer, pStorageServer,
                                               slave_buff, SLAVE_FILE_SIZE,
                                               master_id, "_preview", "jpg",
                                               NULL, 0, slave_id);
    
    int passed = (result == 0 && strlen(slave_id) > 0);
    
    free(slave_buff);
    unlink(master_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("upload_slave_by_buffer", passed);
}

/**
 * Test 3: Upload multiple slaves for one master
 */
static void test_multiple_slaves(ConnectionInfo *pTrackerServer,
                                ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_ids[3][128];
    char master_file[256];
    char slave_files[3][256];
    const char *prefixes[] = {"_thumb", "_medium", "_large"};
    int result;
    int all_passed = 1;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_multi_master_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0) {
        print_test_result("multiple_slaves - file creation", 0);
        return;
    }
    
    // Upload master file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("multiple_slaves - master upload", 0);
        unlink(master_file);
        return;
    }
    
    // Upload 3 slave files with different prefixes
    for (int i = 0; i < 3; i++) {
        snprintf(slave_files[i], sizeof(slave_files[i]), 
                "/tmp/test_slave_%d_%d.jpg", getpid(), i);
        
        if (create_test_file(slave_files[i], SLAVE_FILE_SIZE * (i + 1)) != 0) {
            all_passed = 0;
            break;
        }
        
        result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                                   slave_files[i], master_id,
                                                   prefixes[i], "jpg",
                                                   NULL, 0, slave_ids[i]);
        unlink(slave_files[i]);
        
        if (result != 0 || strlen(slave_ids[i]) == 0) {
            all_passed = 0;
            break;
        }
    }
    
    unlink(master_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    for (int i = 0; i < 3; i++) {
        if (strlen(slave_ids[i]) > 0) {
            storage_delete_file1(pTrackerServer, pStorageServer, slave_ids[i]);
        }
    }
    
    print_test_result("multiple_slaves", all_passed);
}

/**
 * Test 4: Upload slave with metadata
 */
static void test_slave_with_metadata(ConnectionInfo *pTrackerServer,
                                    ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    FDFSMetaData meta_list[2];
    int result;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_meta_master_%d.jpg", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_meta_slave_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("slave_with_metadata - file creation", 0);
        return;
    }
    
    // Upload master file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("slave_with_metadata - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    // Prepare metadata
    strcpy(meta_list[0].name, "width");
    strcpy(meta_list[0].value, "150");
    strcpy(meta_list[1].name, "height");
    strcpy(meta_list[1].value, "150");
    
    // Upload slave with metadata
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "_thumb",
                                               "jpg", meta_list, 2, slave_id);
    
    int passed = (result == 0 && strlen(slave_id) > 0);
    
    unlink(master_file);
    unlink(slave_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("slave_with_metadata", passed);
}

/**
 * Test 5: Upload slave with different file extensions
 */
static void test_slave_different_ext(ConnectionInfo *pTrackerServer,
                                    ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    int result;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_ext_master_%d.mp4", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_ext_slave_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("slave_different_ext - file creation", 0);
        return;
    }
    
    // Upload master video file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("slave_different_ext - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    // Upload slave thumbnail (different extension)
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "_poster",
                                               "jpg", NULL, 0, slave_id);
    
    int passed = (result == 0 && strlen(slave_id) > 0);
    
    unlink(master_file);
    unlink(slave_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("slave_different_ext", passed);
}

/**
 * Test 6: Upload slave with empty prefix
 */
static void test_slave_empty_prefix(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    int result;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_noprefix_master_%d.jpg", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_noprefix_slave_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("slave_empty_prefix - file creation", 0);
        return;
    }
    
    // Upload master file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("slave_empty_prefix - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    // Upload slave with empty prefix
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "",
                                               "jpg", NULL, 0, slave_id);
    
    int passed = (result == 0 && strlen(slave_id) > 0);
    
    unlink(master_file);
    unlink(slave_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("slave_empty_prefix", passed);
}

/**
 * Test 7: Upload large slave file
 */
static void test_slave_large_file(ConnectionInfo *pTrackerServer,
                                 ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    int result;
    int large_size = 5 * 1024 * 1024;  // 5MB
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_large_master_%d.jpg", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_large_slave_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, large_size) != 0) {
        print_test_result("slave_large_file - file creation", 0);
        return;
    }
    
    // Upload master file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("slave_large_file - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    // Upload large slave file
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "_hd",
                                               "jpg", NULL, 0, slave_id);
    
    int passed = (result == 0 && strlen(slave_id) > 0);
    
    unlink(master_file);
    unlink(slave_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("slave_large_file", passed);
}

/**
 * Test 8: Download slave file
 */
static void test_download_slave(ConnectionInfo *pTrackerServer,
                               ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    char download_file[256];
    int result;
    int64_t file_size;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_dl_master_%d.jpg", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_dl_slave_%d.jpg", getpid());
    snprintf(download_file, sizeof(download_file), "/tmp/test_dl_downloaded_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("download_slave - file creation", 0);
        return;
    }
    
    // Upload master and slave
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("download_slave - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "_thumb",
                                               "jpg", NULL, 0, slave_id);
    if (result != 0) {
        print_test_result("download_slave - slave upload", 0);
        unlink(master_file);
        unlink(slave_file);
        storage_delete_file1(pTrackerServer, pStorageServer, master_id);
        return;
    }
    
    // Download slave file
    result = storage_download_file_to_file1(pTrackerServer, pStorageServer,
                                           slave_id, download_file, &file_size);
    
    int passed = (result == 0 && file_size == SLAVE_FILE_SIZE);
    
    unlink(master_file);
    unlink(slave_file);
    unlink(download_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("download_slave", passed);
}

/**
 * Test 9: Error - Upload slave for non-existent master
 */
static void test_slave_nonexistent_master(ConnectionInfo *pTrackerServer,
                                         ConnectionInfo *pStorageServer) {
    char slave_id[128] = {0};
    char slave_file[256];
    int result;
    
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_nomaster_slave_%d.jpg", getpid());
    
    if (create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("slave_nonexistent_master - file creation", 0);
        return;
    }
    
    // Try to upload slave for non-existent master
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file,
                                               "group1/M00/00/00/nonexistent.jpg",
                                               "_thumb", "jpg", NULL, 0, slave_id);
    
    unlink(slave_file);
    
    // Should fail
    print_test_result("slave_nonexistent_master", result != 0);
}

/**
 * Test 10: Error - Upload slave with invalid master ID
 */
static void test_slave_invalid_master_id(ConnectionInfo *pTrackerServer,
                                        ConnectionInfo *pStorageServer) {
    char slave_id[128] = {0};
    char slave_file[256];
    int result;
    
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_invalid_slave_%d.jpg", getpid());
    
    if (create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("slave_invalid_master_id - file creation", 0);
        return;
    }
    
    // Try to upload slave with invalid master ID format
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, "invalid_format",
                                               "_thumb", "jpg", NULL, 0, slave_id);
    
    unlink(slave_file);
    
    // Should fail
    print_test_result("slave_invalid_master_id", result != 0);
}

/**
 * Test 11: Upload slave with special characters in prefix
 */
static void test_slave_special_prefix(ConnectionInfo *pTrackerServer,
                                     ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    int result;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_special_master_%d.jpg", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_special_slave_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("slave_special_prefix - file creation", 0);
        return;
    }
    
    // Upload master file
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("slave_special_prefix - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    // Upload slave with special characters in prefix
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "_thumb-150x150",
                                               "jpg", NULL, 0, slave_id);
    
    int passed = (result == 0 && strlen(slave_id) > 0);
    
    unlink(master_file);
    unlink(slave_file);
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    print_test_result("slave_special_prefix", passed);
}

/**
 * Test 12: Delete master and verify slave still exists
 */
static void test_slave_after_master_delete(ConnectionInfo *pTrackerServer,
                                          ConnectionInfo *pStorageServer) {
    char master_id[128] = {0};
    char slave_id[128] = {0};
    char master_file[256];
    char slave_file[256];
    int result;
    int slave_exists;
    
    snprintf(master_file, sizeof(master_file), "/tmp/test_del_master_%d.jpg", getpid());
    snprintf(slave_file, sizeof(slave_file), "/tmp/test_del_slave_%d.jpg", getpid());
    
    if (create_test_file(master_file, MASTER_FILE_SIZE) != 0 ||
        create_test_file(slave_file, SLAVE_FILE_SIZE) != 0) {
        print_test_result("slave_after_master_delete - file creation", 0);
        return;
    }
    
    // Upload master and slave
    result = upload_file(pTrackerServer, pStorageServer, master_file,
                        master_id, sizeof(master_id));
    if (result != 0) {
        print_test_result("slave_after_master_delete - master upload", 0);
        unlink(master_file);
        unlink(slave_file);
        return;
    }
    
    result = storage_upload_slave_by_filename1(pTrackerServer, pStorageServer,
                                               slave_file, master_id, "_thumb",
                                               "jpg", NULL, 0, slave_id);
    if (result != 0) {
        print_test_result("slave_after_master_delete - slave upload", 0);
        unlink(master_file);
        unlink(slave_file);
        storage_delete_file1(pTrackerServer, pStorageServer, master_id);
        return;
    }
    
    // Delete master file
    storage_delete_file1(pTrackerServer, pStorageServer, master_id);
    
    // Check if slave still exists
    storage_file_exist1(pTrackerServer, pStorageServer, slave_id, &slave_exists);
    
    unlink(master_file);
    unlink(slave_file);
    storage_delete_file1(pTrackerServer, pStorageServer, slave_id);
    
    // Slave should still exist after master deletion
    print_test_result("slave_after_master_delete", slave_exists == 1);
}

int main(int argc, char *argv[]) {
    char *conf_filename;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    int result;
    
    printf("=== FastDFS Slave File Operations Test Suite ===\n\n");
    
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
    
    printf("Running slave file operation tests...\n\n");
    
    test_upload_slave_by_filename(pTrackerServer, pStorageServer);
    test_upload_slave_by_buffer(pTrackerServer, pStorageServer);
    test_multiple_slaves(pTrackerServer, pStorageServer);
    test_slave_with_metadata(pTrackerServer, pStorageServer);
    test_slave_different_ext(pTrackerServer, pStorageServer);
    test_slave_empty_prefix(pTrackerServer, pStorageServer);
    test_slave_large_file(pTrackerServer, pStorageServer);
    test_download_slave(pTrackerServer, pStorageServer);
    test_slave_nonexistent_master(pTrackerServer, pStorageServer);
    test_slave_invalid_master_id(pTrackerServer, pStorageServer);
    test_slave_special_prefix(pTrackerServer, pStorageServer);
    test_slave_after_master_delete(pTrackerServer, pStorageServer);
    
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
