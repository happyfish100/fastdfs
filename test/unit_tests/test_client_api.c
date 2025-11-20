/**
 * ==============================================================================
 * FastDFS Client API Unit Tests
 * ==============================================================================
 * Copyright (C) 2008 Happy Fish / YuQing
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
 *
 * PURPOSE:
 *   Comprehensive unit tests for FastDFS client API functionality
 *
 * TEST COVERAGE:
 *   - Client initialization and configuration validation
 *   - Tracker server connection management
 *   - File upload operations (buffer-based)
 *   - File download operations (to buffer)
 *   - Metadata operations (set/get)
 *   - File information queries
 *   - File deletion operations
 *
 * USAGE:
 *   ./test_client_api [config_file]
 *
 *   If config_file is not specified, uses /etc/fdfs/client.conf by default
 *
 * REQUIREMENTS:
 *   - FastDFS tracker and storage servers must be running
 *   - Valid client.conf configuration file
 *   - Network connectivity to FastDFS servers
 *
 * EXIT CODES:
 *   0 - All tests passed
 *   1 - One or more tests failed
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "fastdfs/fdfs_client.h"

/* ==============================================================================
 * Test Configuration Constants
 * ============================================================================== */

#define TEST_CONFIG_FILE    "/etc/fdfs/client.conf"  /* Default config path */
#define TEST_FILE_SIZE      1024                      /* Test file size (1KB) */
#define TEST_GROUP_NAME     "group1"                  /* Default group name */
#define MAX_FILE_ID_LEN     128                       /* Max file ID length */

/* ==============================================================================
 * Test Result Tracking
 * ============================================================================== */

/**
 * Structure to track test execution results
 * Maintains counts of total, passed, failed, and skipped tests
 */
typedef struct {
    int total;      /* Total number of tests executed */
    int passed;     /* Number of tests that passed */
    int failed;     /* Number of tests that failed */
    int skipped;    /* Number of tests skipped (e.g., server unavailable) */
} TestResults;

/* Global test results tracker */
static TestResults g_test_results = {0, 0, 0, 0};

/* Global variables to track uploaded test file for subsequent tests */
static char g_test_file_id[MAX_FILE_ID_LEN] = {0};                     /* Full file ID */
static char g_test_group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};     /* Group name */

/* ==============================================================================
 * ANSI Color Codes for Terminal Output
 * ============================================================================== */

#define COLOR_RESET   "\033[0m"      /* Reset to default color */
#define COLOR_RED     "\033[31m"     /* Red for failures */
#define COLOR_GREEN   "\033[32m"     /* Green for success */
#define COLOR_YELLOW  "\033[33m"     /* Yellow for warnings/skipped */
#define COLOR_BLUE    "\033[34m"     /* Blue for headers */
#define COLOR_CYAN    "\033[36m"     /* Cyan for section titles */

/* ==============================================================================
 * Test Assertion Macros
 * ==============================================================================
 * These macros provide convenient assertion checking with automatic error
 * reporting. All macros return -1 on failure to indicate test failure.
 * ============================================================================== */

/**
 * Assert that two values are equal
 * Usage: ASSERT_EQ(result, 0, "Operation should succeed")
 */
#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            printf(COLOR_RED "  ✗ FAILED: %s (expected: %d, got: %d)" COLOR_RESET "\n", \
                   msg, expected, actual); \
            return -1; \
        } \
    } while(0)

/**
 * Assert that two values are NOT equal
 * Usage: ASSERT_NE(result, 0, "Operation should fail")
 */
#define ASSERT_NE(actual, not_expected, msg) \
    do { \
        if ((actual) == (not_expected)) { \
            printf(COLOR_RED "  ✗ FAILED: %s (should not be: %d)" COLOR_RESET "\n", \
                   msg, not_expected); \
            return -1; \
        } \
    } while(0)

/**
 * Assert that a pointer is NOT NULL
 * Usage: ASSERT_NOT_NULL(buffer, "Buffer allocation failed")
 */
#define ASSERT_NOT_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            printf(COLOR_RED "  ✗ FAILED: %s (pointer is NULL)" COLOR_RESET "\n", msg); \
            return -1; \
        } \
    } while(0)

/* ==============================================================================
 * Helper Functions
 * ============================================================================== */

/**
 * Generate test data with repeating pattern
 * Fills buffer with characters A-Z repeated cyclically
 * 
 * @param buffer  Buffer to fill with test data
 * @param size    Size of buffer in bytes
 */
static void generate_test_data(char *buffer, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        buffer[i] = (char)('A' + (i % 26));
    }
}

/**
 * Print formatted test header
 * Displays test name with visual separator
 * 
 * @param test_name  Name of the test being executed
 */
static void print_test_header(const char *test_name)
{
    printf("\n" COLOR_CYAN "TEST: %s" COLOR_RESET "\n", test_name);
}

/**
 * Print test result and update statistics
 * Automatically tracks test results in global statistics
 * 
 * @param test_name  Name of the test
 * @param result     Test result code:
 *                   0  = passed
 *                   -1 = failed
 *                   -2 = skipped (e.g., server unavailable)
 */
static void print_test_result(const char *test_name, int result)
{
    g_test_results.total++;
    if (result == 0) {
        g_test_results.passed++;
        printf(COLOR_GREEN "  ✓ PASSED: %s" COLOR_RESET "\n", test_name);
    } else if (result == -2) {
        g_test_results.skipped++;
        printf(COLOR_YELLOW "  ⊘ SKIPPED: %s" COLOR_RESET "\n", test_name);
    } else {
        g_test_results.failed++;
        printf(COLOR_RED "  ✗ FAILED: %s (error code: %d)" COLOR_RESET "\n", 
               test_name, result);
    }
}

/* ==============================================================================
 * Test Cases: Client Initialization
 * ==============================================================================
 * Tests for fdfs_client_init() and fdfs_client_destroy()
 * Validates proper handling of configuration files and initialization
 * ============================================================================== */

/**
 * TEST: Client initialization with valid configuration file
 * 
 * Verifies that the client can successfully initialize with a valid config file.
 * If the config file doesn't exist, the test is skipped rather than failed.
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_client_init_valid_config(void)
{
    int result;
    
    print_test_header("Client Initialization - Valid Config");
    
    result = fdfs_client_init(TEST_CONFIG_FILE);
    if (result != 0 && result != ENOENT) {
        printf("  Info: Config file may not exist at %s\n", TEST_CONFIG_FILE);
        return -2;
    }
    
    ASSERT_EQ(result, 0, "Client initialization should succeed");
    printf("  ✓ Client initialized successfully\n");
    return 0;
}

/**
 * TEST: Client initialization with NULL configuration file
 * 
 * Validates that the client properly rejects NULL configuration parameter.
 * This ensures proper error handling for invalid input.
 * 
 * @return 0 on success (properly rejected), -1 on failure
 */
static int test_client_init_null_config(void)
{
    int result;
    
    print_test_header("Client Initialization - NULL Config");
    
    result = fdfs_client_init(NULL);
    ASSERT_NE(result, 0, "Client init with NULL config should fail");
    
    printf("  ✓ Correctly rejected NULL config (error: %d)\n", result);
    return 0;
}

/* ==============================================================================
 * Test Cases: File Upload Operations
 * ==============================================================================
 * Tests for storage_upload_by_filebuff() and related upload functions
 * Validates file upload with various scenarios and error conditions
 * ============================================================================== */

/**
 * TEST: Upload file from memory buffer
 * 
 * Tests the basic file upload functionality using a memory buffer.
 * Generates test data, uploads it to FastDFS, and stores the file ID
 * for use in subsequent tests (download, metadata, delete).
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_upload_file_by_buffer(void)
{
    int result;
    char *file_buff;
    char remote_filename[MAX_FILE_ID_LEN];
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer = NULL;
    
    print_test_header("File Upload - By Buffer");
    
    file_buff = (char *)malloc(TEST_FILE_SIZE);
    ASSERT_NOT_NULL(file_buff, "Memory allocation for test buffer");
    generate_test_data(file_buff, TEST_FILE_SIZE);
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("  Skipping: Cannot connect to tracker server\n");
        free(file_buff);
        return -2;
    }
    
    g_test_group_name[0] = '\0';
    result = storage_upload_by_filebuff(pTrackerServer, pStorageServer,
                                       0, file_buff, TEST_FILE_SIZE,
                                       "txt", NULL, 0,
                                       g_test_group_name, remote_filename);
    
    if (result == 0) {
        snprintf(g_test_file_id, sizeof(g_test_file_id), 
                "%s%c%s", g_test_group_name, FDFS_FILE_ID_SEPERATOR, remote_filename);
        printf("  ✓ File uploaded: %s\n", g_test_file_id);
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    free(file_buff);
    
    ASSERT_EQ(result, 0, "File upload should succeed");
    return 0;
}

/* ==============================================================================
 * Test Cases: File Download Operations
 * ==============================================================================
 * Tests for storage_download_file_to_buff() and related download functions
 * Validates file download and content verification
 * ============================================================================== */

/**
 * TEST: Download file to memory buffer
 * 
 * Downloads the previously uploaded test file and verifies:
 * - Download succeeds without errors
 * - Downloaded size matches uploaded size
 * - Memory is properly allocated and freed
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_download_file_to_buffer(void)
{
    int result;
    char *file_buff = NULL;
    int64_t file_size = 0;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer = NULL;
    
    print_test_header("File Download - To Buffer");
    
    if (g_test_file_id[0] == '\0') {
        printf("  Skipping: No test file uploaded yet\n");
        return -2;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("  Skipping: Cannot connect to tracker server\n");
        return -2;
    }
    
    FDFS_SPLIT_GROUP_NAME_AND_FILENAME(g_test_file_id);
    
    result = storage_download_file_to_buff(pTrackerServer, pStorageServer,
                                          group_name, filename,
                                          &file_buff, &file_size);
    
    if (result == 0) {
        printf("  ✓ Downloaded %lld bytes\n", (long long)file_size);
        ASSERT_EQ(file_size, TEST_FILE_SIZE, "Downloaded size should match");
        
        if (file_buff != NULL) {
            free(file_buff);
        }
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    
    ASSERT_EQ(result, 0, "File download should succeed");
    return 0;
}

/* ==============================================================================
 * Test Cases: Metadata Operations
 * ==============================================================================
 * Tests for storage_set_metadata() and storage_get_metadata()
 * Validates metadata storage and retrieval functionality
 * ============================================================================== */

/**
 * TEST: Set file metadata
 * 
 * Tests setting metadata key-value pairs on an uploaded file.
 * Uses OVERWRITE mode to replace any existing metadata.
 * Sets test metadata: author, version
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_set_metadata(void)
{
    int result;
    FDFSMetaData meta_list[2];
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer = NULL;
    
    print_test_header("Metadata - Set");
    
    if (g_test_file_id[0] == '\0') {
        printf("  Skipping: No test file uploaded yet\n");
        return -2;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("  Skipping: Cannot connect to tracker server\n");
        return -2;
    }
    
    snprintf(meta_list[0].name, sizeof(meta_list[0].name), "author");
    snprintf(meta_list[0].value, sizeof(meta_list[0].value), "test_user");
    
    snprintf(meta_list[1].name, sizeof(meta_list[1].name), "version");
    snprintf(meta_list[1].value, sizeof(meta_list[1].value), "1.0");
    
    FDFS_SPLIT_GROUP_NAME_AND_FILENAME(g_test_file_id);
    
    result = storage_set_metadata(pTrackerServer, pStorageServer,
                                  group_name, filename,
                                  meta_list, 2,
                                  STORAGE_SET_METADATA_FLAG_OVERWRITE);
    
    if (result == 0) {
        printf("  ✓ Metadata set (2 items)\n");
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    
    ASSERT_EQ(result, 0, "Set metadata should succeed");
    return 0;
}

/**
 * TEST: Get file metadata
 * 
 * Retrieves and displays metadata previously set on the test file.
 * Validates that metadata can be successfully retrieved and
 * displays all key-value pairs for verification.
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_get_metadata(void)
{
    int result;
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer = NULL;
    int i;
    
    print_test_header("Metadata - Get");
    
    if (g_test_file_id[0] == '\0') {
        printf("  Skipping: No test file uploaded yet\n");
        return -2;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("  Skipping: Cannot connect to tracker server\n");
        return -2;
    }
    
    FDFS_SPLIT_GROUP_NAME_AND_FILENAME(g_test_file_id);
    
    result = storage_get_metadata(pTrackerServer, pStorageServer,
                                  group_name, filename,
                                  &meta_list, &meta_count);
    
    if (result == 0) {
        printf("  ✓ Retrieved %d metadata items\n", meta_count);
        
        for (i = 0; i < meta_count; i++) {
            printf("    %s = %s\n", meta_list[i].name, meta_list[i].value);
        }
        
        if (meta_list != NULL) {
            free(meta_list);
        }
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    
    ASSERT_EQ(result, 0, "Get metadata should succeed");
    return 0;
}

/* ==============================================================================
 * Test Cases: File Information
 * ==============================================================================
 * Tests for storage_query_file_info() and related info functions
 * Validates file information retrieval and accuracy
 * ============================================================================== */

/**
 * TEST: Query file information
 * 
 * Retrieves detailed file information including:
 * - File size
 * - Creation timestamp
 * - Source storage server IP
 * - CRC32 checksum
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_query_file_info(void)
{
    int result;
    FDFSFileInfo file_info;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer = NULL;
    
    print_test_header("File Info - Query");
    
    if (g_test_file_id[0] == '\0') {
        printf("  Skipping: No test file uploaded yet\n");
        return -2;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("  Skipping: Cannot connect to tracker server\n");
        return -2;
    }
    
    FDFS_SPLIT_GROUP_NAME_AND_FILENAME(g_test_file_id);
    
    memset(&file_info, 0, sizeof(file_info));
    result = storage_query_file_info(pTrackerServer, pStorageServer,
                                    group_name, filename, &file_info);
    
    if (result == 0) {
        printf("  ✓ File size: %lld bytes\n", (long long)file_info.file_size);
        printf("    Source IP: %s\n", file_info.source_ip_addr);
        
        ASSERT_EQ(file_info.file_size, TEST_FILE_SIZE, "File size should match");
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    
    ASSERT_EQ(result, 0, "Query file info should succeed");
    return 0;
}

/* ==============================================================================
 * Test Cases: File Deletion
 * ==============================================================================
 * Tests for storage_delete_file()
 * Validates file deletion and cleanup
 * ============================================================================== */

/**
 * TEST: Delete file from storage
 * 
 * Deletes the test file uploaded earlier.
 * Clears the global file ID after successful deletion.
 * This should be one of the last tests to run.
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_delete_file(void)
{
    int result;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer = NULL;
    
    print_test_header("File Delete");
    
    if (g_test_file_id[0] == '\0') {
        printf("  Skipping: No test file uploaded yet\n");
        return -2;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        printf("  Skipping: Cannot connect to tracker server\n");
        return -2;
    }
    
    FDFS_SPLIT_GROUP_NAME_AND_FILENAME(g_test_file_id);
    
    result = storage_delete_file(pTrackerServer, pStorageServer,
                                group_name, filename);
    
    if (result == 0) {
        printf("  ✓ File deleted: %s\n", g_test_file_id);
        g_test_file_id[0] = '\0';
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    
    ASSERT_EQ(result, 0, "File delete should succeed");
    return 0;
}

/* ==============================================================================
 * Test Cases: Connection Management
 * ==============================================================================
 * Tests for tracker_get_connection() and connection handling
 * Validates tracker server connectivity
 * ============================================================================== */

/**
 * TEST: Get tracker server connection
 * 
 * Tests basic tracker server connection establishment.
 * Displays connection details (IP address and port).
 * Properly disconnects after verification.
 * 
 * @return 0 on success, -1 on failure, -2 if skipped
 */
static int test_tracker_get_connection(void)
{
    ConnectionInfo *pTrackerServer;
    
    print_test_header("Connection - Get Tracker");
    
    pTrackerServer = tracker_get_connection();
    
    if (pTrackerServer == NULL) {
        printf("  Skipping: Cannot connect to tracker server\n");
        return -2;
    }
    
    printf("  ✓ Connected to %s:%d\n", pTrackerServer->ip_addr, pTrackerServer->port);
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    
    return 0;
}

/* ==============================================================================
 * Test Runner Infrastructure
 * ============================================================================== */

/**
 * Function pointer type for test functions
 * All test functions must match this signature
 */
typedef int (*TestFunction)(void);

/**
 * Test case structure
 * Associates a test name with its implementation function
 */
typedef struct {
    const char *name;       /* Human-readable test name */
    TestFunction func;      /* Test function pointer */
} TestCase;

/**
 * Test suite definition
 * Array of all test cases to be executed
 * Tests are run in the order defined here
 */
static TestCase test_cases[] = {
    {"Client Init - Valid Config", test_client_init_valid_config},
    {"Client Init - NULL Config", test_client_init_null_config},
    {"Get Tracker Connection", test_tracker_get_connection},
    {"Upload File - By Buffer", test_upload_file_by_buffer},
    {"Download File - To Buffer", test_download_file_to_buffer},
    {"Set Metadata", test_set_metadata},
    {"Get Metadata", test_get_metadata},
    {"Query File Info", test_query_file_info},
    {"Delete File", test_delete_file},
};

/**
 * Print test execution summary
 * 
 * Displays comprehensive test results including:
 * - Total test count
 * - Passed, failed, and skipped counts
 * - Pass rate percentage (excluding skipped tests)
 * - Overall pass/fail status
 * 
 * Uses color coding for visual clarity
 */
static void print_summary(void)
{
    double pass_rate = 0.0;
    
    printf("\n" COLOR_CYAN "═══════════════════════════════════════════════════════════" COLOR_RESET "\n");
    printf(COLOR_CYAN "  TEST SUMMARY" COLOR_RESET "\n");
    printf(COLOR_CYAN "═══════════════════════════════════════════════════════════" COLOR_RESET "\n");
    printf("\n");
    printf("  Total Tests:   %d\n", g_test_results.total);
    printf(COLOR_GREEN "  Passed:        %d" COLOR_RESET "\n", g_test_results.passed);
    printf(COLOR_RED "  Failed:        %d" COLOR_RESET "\n", g_test_results.failed);
    printf(COLOR_YELLOW "  Skipped:       %d" COLOR_RESET "\n", g_test_results.skipped);
    
    /* Calculate pass rate excluding skipped tests */
    if (g_test_results.total > 0) {
        pass_rate = (double)g_test_results.passed / 
                   (g_test_results.total - g_test_results.skipped) * 100.0;
        printf("\n  Pass Rate:     %.1f%%\n", pass_rate);
    }
    
    printf("\n" COLOR_CYAN "═══════════════════════════════════════════════════════════" COLOR_RESET "\n");
    
    if (g_test_results.failed == 0) {
        printf(COLOR_GREEN "  ALL TESTS PASSED!" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "  SOME TESTS FAILED!" COLOR_RESET "\n");
    }
    printf(COLOR_CYAN "═══════════════════════════════════════════════════════════" COLOR_RESET "\n\n");
}

/* ==============================================================================
 * Main Entry Point
 * ============================================================================== */

/**
 * Main test runner
 * 
 * Executes all registered test cases in sequence and reports results.
 * 
 * Command line arguments:
 *   argv[1] - Optional path to client configuration file
 *             If not provided, uses TEST_CONFIG_FILE default
 * 
 * Process:
 *   1. Parse command line arguments
 *   2. Initialize logging system
 *   3. Execute each test case in order
 *   4. Track and display results
 *   5. Print summary statistics
 *   6. Clean up resources
 * 
 * Exit codes:
 *   0 - All tests passed (or only skipped tests)
 *   1 - One or more tests failed
 * 
 * @param argc  Argument count
 * @param argv  Argument vector
 * @return Exit code indicating overall test result
 */
int main(int argc, char *argv[])
{
    int i;
    int result;
    const char *config_file = TEST_CONFIG_FILE;
    
    /* Print banner */
    printf("\n" COLOR_BLUE "╔═══════════════════════════════════════════════════════════╗" COLOR_RESET "\n");
    printf(COLOR_BLUE "║       FastDFS Client API Unit Tests                      ║" COLOR_RESET "\n");
    printf(COLOR_BLUE "╚═══════════════════════════════════════════════════════════╝" COLOR_RESET "\n");
    
    /* Parse command line arguments */
    if (argc >= 2) {
        config_file = argv[1];
    }
    
    printf("\nConfiguration: %s\n", config_file);
    
    /* Initialize FastDFS logging system */
    log_init();
    
    /* Execute all test cases */
    for (i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        result = test_cases[i].func();
        print_test_result(test_cases[i].name, result);
    }
    
    /* Display summary of test results */
    print_summary();
    
    /* Clean up FastDFS client resources */
    fdfs_client_destroy();
    
    /* Return exit code: 0 for success, 1 if any tests failed */
    return (g_test_results.failed > 0) ? 1 : 0;
}
