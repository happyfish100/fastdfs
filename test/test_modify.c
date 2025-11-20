/**
 * Comprehensive test suite for FastDFS modify operations
 * Tests storage_modify_by_filename1, storage_modify_by_filebuff1, and storage_modify_by_callback1
 * 
 * Modify operations allow updating existing file content at specific offsets
 * without creating a new file or appending to the end.
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

// Test configuration
#define TEST_FILE_SIZE 1024
#define MODIFY_OFFSET 100
#define MODIFY_SIZE 50
#define LARGE_MODIFY_SIZE 500

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Helper function to print test results
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

// Helper function to create a test file with known content
static int create_test_file(const char *filename, int size) {
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        return -1;
    }
    
    // Fill with pattern: 'A', 'B', 'C', ... repeating
    for (int i = 0; i < size; i++) {
        fputc('A' + (i % 26), fp);
    }
    
    fclose(fp);
    return 0;
}

// Helper function to verify file content at offset
static int verify_file_content(const char *filename, int64_t offset, 
                               const char *expected, int length) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        return -1;
    }
    
    if (fseek(fp, offset, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    
    char *buffer = (char *)malloc(length);
    if (buffer == NULL) {
        fclose(fp);
        return -1;
    }
    
    int bytes_read = fread(buffer, 1, length, fp);
    fclose(fp);
    
    if (bytes_read != length) {
        free(buffer);
        return -1;
    }
    
    int result = memcmp(buffer, expected, length);
    free(buffer);
    return result;
}

/**
 * Test 1: Basic modify by filename
 * Uploads a file, then modifies content at a specific offset
 */
static void test_modify_by_filename_basic(ConnectionInfo *pTrackerServer,
                                         ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_file[256];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_basic_%d.dat", getpid());
    snprintf(modify_file, sizeof(modify_file), "/tmp/test_modify_data_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_by_filename_basic - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_by_filename_basic - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Create modification data
    if (create_test_file(modify_file, MODIFY_SIZE) != 0) {
        print_test_result("modify_by_filename_basic - modify data creation", 0);
        unlink(local_file);
        return;
    }
    
    // Modify file at offset
    result = storage_modify_by_filename1(pTrackerServer, pStorageServer,
                                        modify_file, MODIFY_OFFSET, file_id);
    
    // Cleanup
    unlink(local_file);
    unlink(modify_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("modify_by_filename_basic", result == 0);
}

/**
 * Test 2: Modify by filebuff
 * Tests modifying file content using a memory buffer
 */
static void test_modify_by_filebuff(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_data[MODIFY_SIZE];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_buff_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_by_filebuff - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_by_filebuff - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Prepare modification data
    memset(modify_data, 'X', sizeof(modify_data));
    
    // Modify file using buffer
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        MODIFY_OFFSET, file_id);
    
    // Cleanup
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("modify_by_filebuff", result == 0);
}

/**
 * Test 3: Modify at offset zero
 * Tests modifying content at the beginning of the file
 */
static void test_modify_at_offset_zero(ConnectionInfo *pTrackerServer,
                                       ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_data[MODIFY_SIZE];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_zero_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_at_offset_zero - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_at_offset_zero - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Prepare modification data
    memset(modify_data, 'Z', sizeof(modify_data));
    
    // Modify at offset 0
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        0, file_id);
    
    // Cleanup
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("modify_at_offset_zero", result == 0);
}

/**
 * Test 4: Modify near end of file
 * Tests modifying content near the end of the file
 */
static void test_modify_near_end(ConnectionInfo *pTrackerServer,
                                ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_data[MODIFY_SIZE];
    int result;
    int64_t offset = TEST_FILE_SIZE - MODIFY_SIZE;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_end_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_near_end - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_near_end - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Prepare modification data
    memset(modify_data, 'Y', sizeof(modify_data));
    
    // Modify near end
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        offset, file_id);
    
    // Cleanup
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("modify_near_end", result == 0);
}

/**
 * Test 5: Multiple sequential modifications
 * Tests modifying different parts of the file in sequence
 */
static void test_multiple_modifications(ConnectionInfo *pTrackerServer,
                                       ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_data[MODIFY_SIZE];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_multi_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("multiple_modifications - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("multiple_modifications - upload", 0);
        unlink(local_file);
        return;
    }
    
    // First modification
    memset(modify_data, '1', sizeof(modify_data));
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        100, file_id);
    if (result != 0) {
        unlink(local_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        print_test_result("multiple_modifications", 0);
        return;
    }
    
    // Second modification
    memset(modify_data, '2', sizeof(modify_data));
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        200, file_id);
    if (result != 0) {
        unlink(local_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        print_test_result("multiple_modifications", 0);
        return;
    }
    
    // Third modification
    memset(modify_data, '3', sizeof(modify_data));
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        300, file_id);
    
    // Cleanup
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("multiple_modifications", result == 0);
}

/**
 * Test 6: Modify with large data
 * Tests modifying with a larger chunk of data
 */
static void test_modify_large_data(ConnectionInfo *pTrackerServer,
                                  ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char *modify_data;
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_large_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_large_data - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_large_data - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Allocate large modification data
    modify_data = (char *)malloc(LARGE_MODIFY_SIZE);
    if (modify_data == NULL) {
        print_test_result("modify_large_data - allocation", 0);
        unlink(local_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        return;
    }
    
    memset(modify_data, 'L', LARGE_MODIFY_SIZE);
    
    // Modify with large data
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, LARGE_MODIFY_SIZE,
                                        50, file_id);
    
    // Cleanup
    free(modify_data);
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("modify_large_data", result == 0);
}

/**
 * Test 7: Modify with overlapping regions
 * Tests modifying overlapping regions of the file
 */
static void test_modify_overlapping(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_data[MODIFY_SIZE];
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_overlap_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_overlapping - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_overlapping - upload", 0);
        unlink(local_file);
        return;
    }
    
    // First modification
    memset(modify_data, 'A', sizeof(modify_data));
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        100, file_id);
    if (result != 0) {
        unlink(local_file);
        storage_delete_file1(pTrackerServer, pStorageServer, file_id);
        print_test_result("modify_overlapping", 0);
        return;
    }
    
    // Second modification overlapping the first
    memset(modify_data, 'B', sizeof(modify_data));
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        125, file_id);  // Overlaps with first
    
    // Cleanup
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("modify_overlapping", result == 0);
}

/**
 * Test 8: Error case - invalid file ID
 * Tests error handling with invalid file ID
 */
static void test_modify_invalid_file_id(ConnectionInfo *pTrackerServer,
                                       ConnectionInfo *pStorageServer) {
    char modify_data[MODIFY_SIZE];
    int result;
    
    memset(modify_data, 'X', sizeof(modify_data));
    
    // Try to modify non-existent file
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        0, "group1/M00/00/00/invalid_file_id");
    
    // Should fail
    print_test_result("modify_invalid_file_id", result != 0);
}

/**
 * Test 9: Error case - offset beyond file size
 * Tests error handling when offset exceeds file size
 */
static void test_modify_offset_beyond_size(ConnectionInfo *pTrackerServer,
                                          ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_data[MODIFY_SIZE];
    int result;
    int64_t invalid_offset = TEST_FILE_SIZE + 1000;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_beyond_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_offset_beyond_size - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_offset_beyond_size - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Try to modify beyond file size
    memset(modify_data, 'X', sizeof(modify_data));
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        modify_data, sizeof(modify_data),
                                        invalid_offset, file_id);
    
    // Cleanup
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    // Should fail
    print_test_result("modify_offset_beyond_size", result != 0);
}

/**
 * Test 10: Modify with single byte
 * Tests modifying just one byte
 */
static void test_modify_single_byte(ConnectionInfo *pTrackerServer,
                                   ConnectionInfo *pStorageServer) {
    char file_id[128] = {0};
    char local_file[256];
    char modify_data = 'S';
    int result;
    
    snprintf(local_file, sizeof(local_file), "/tmp/test_modify_byte_%d.dat", getpid());
    
    // Create initial file
    if (create_test_file(local_file, TEST_FILE_SIZE) != 0) {
        print_test_result("modify_single_byte - file creation", 0);
        return;
    }
    
    // Upload initial file
    result = upload_file(pTrackerServer, pStorageServer, local_file, file_id, sizeof(file_id));
    if (result != 0) {
        print_test_result("modify_single_byte - upload", 0);
        unlink(local_file);
        return;
    }
    
    // Modify single byte
    result = storage_modify_by_filebuff1(pTrackerServer, pStorageServer,
                                        &modify_data, 1,
                                        512, file_id);
    
    // Cleanup
    unlink(local_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    print_test_result("modify_single_byte", result == 0);
}

int main(int argc, char *argv[]) {
    char *conf_filename;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    int result;
    
    printf("=== FastDFS Modify Operations Test Suite ===\n\n");
    
    // Get config file
    if (argc < 2) {
        conf_filename = "/etc/fdfs/client.conf";
    } else {
        conf_filename = argv[1];
    }
    
    // Initialize
    log_init();
    g_log_context.log_level = LOG_ERR;
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        printf("ERROR: Failed to initialize FastDFS client\n");
        return result;
    }
    
    // Get tracker connection
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("ERROR: Failed to connect to tracker server\n");
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    // Get storage connection
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        printf("ERROR: Failed to connect to storage server\n");
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    printf("Running modify operation tests...\n\n");
    
    // Run all tests
    test_modify_by_filename_basic(pTrackerServer, pStorageServer);
    test_modify_by_filebuff(pTrackerServer, pStorageServer);
    test_modify_at_offset_zero(pTrackerServer, pStorageServer);
    test_modify_near_end(pTrackerServer, pStorageServer);
    test_multiple_modifications(pTrackerServer, pStorageServer);
    test_modify_large_data(pTrackerServer, pStorageServer);
    test_modify_overlapping(pTrackerServer, pStorageServer);
    test_modify_invalid_file_id(pTrackerServer, pStorageServer);
    test_modify_offset_beyond_size(pTrackerServer, pStorageServer);
    test_modify_single_byte(pTrackerServer, pStorageServer);
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    // Cleanup
    tracker_disconnect_server_ex(pStorageServer, true);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return tests_failed > 0 ? 1 : 0;
}
