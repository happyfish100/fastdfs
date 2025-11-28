/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_network_diag.h
* Header file for FastDFS network diagnostics utilities
*/

#ifndef FDFS_NETWORK_DIAG_H
#define FDFS_NETWORK_DIAG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum limits */
#define ND_MAX_SERVERS 64
#define ND_MAX_LINE_LENGTH 1024
#define ND_MAX_HOSTNAME 256
#define ND_MAX_MESSAGE 512

/* Default ports */
#define ND_DEFAULT_TRACKER_PORT 22122
#define ND_DEFAULT_STORAGE_PORT 23000
#define ND_DEFAULT_HTTP_PORT 8080

/* Default timeouts in milliseconds */
#define ND_DEFAULT_CONNECT_TIMEOUT_MS 5000
#define ND_DEFAULT_READ_TIMEOUT_MS 10000
#define ND_DEFAULT_WRITE_TIMEOUT_MS 10000

/* Test parameters */
#define ND_DEFAULT_PING_COUNT 5
#define ND_DEFAULT_BANDWIDTH_SIZE (1024 * 1024)  /* 1MB */
#define ND_DEFAULT_PACKET_SIZE 1400

/* Diagnostic result levels */
#define ND_LEVEL_OK 0
#define ND_LEVEL_INFO 1
#define ND_LEVEL_WARNING 2
#define ND_LEVEL_ERROR 3
#define ND_LEVEL_CRITICAL 4

/* Server types */
#define ND_SERVER_TRACKER 1
#define ND_SERVER_STORAGE 2
#define ND_SERVER_HTTP 3

/* Test types */
#define ND_TEST_CONNECTIVITY 1
#define ND_TEST_LATENCY 2
#define ND_TEST_BANDWIDTH 3
#define ND_TEST_DNS 4
#define ND_TEST_PORT_SCAN 5
#define ND_TEST_MTU 6
#define ND_TEST_ALL 0xFF

/**
 * Server information structure
 */
typedef struct nd_server_info {
    char host[ND_MAX_HOSTNAME];
    char ip[64];
    int port;
    int server_type;
    int is_reachable;
    double latency_ms;
} NDServerInfo;

/**
 * Latency statistics structure
 */
typedef struct nd_latency_stats {
    double min_ms;
    double max_ms;
    double avg_ms;
    double stddev_ms;
    int samples;
    int lost;
    double loss_percent;
} NDLatencyStats;

/**
 * Bandwidth test result structure
 */
typedef struct nd_bandwidth_result {
    double upload_mbps;
    double download_mbps;
    long bytes_sent;
    long bytes_received;
    double duration_sec;
} NDBandwidthResult;

/**
 * DNS resolution result structure
 */
typedef struct nd_dns_result {
    char hostname[ND_MAX_HOSTNAME];
    char ip_addresses[8][64];
    int ip_count;
    double resolution_time_ms;
    int success;
} NDDnsResult;

/**
 * Port scan result structure
 */
typedef struct nd_port_result {
    int port;
    int is_open;
    char service[64];
    double response_time_ms;
} NDPortResult;

/**
 * MTU discovery result structure
 */
typedef struct nd_mtu_result {
    int mtu_size;
    int path_mtu;
    int fragmentation_needed;
} NDMtuResult;

/**
 * Diagnostic result structure
 */
typedef struct nd_diagnostic_result {
    int level;
    int test_type;
    char server[ND_MAX_HOSTNAME];
    int port;
    char message[ND_MAX_MESSAGE];
    char suggestion[ND_MAX_MESSAGE];
    struct timeval timestamp;
} NDDiagnosticResult;

/**
 * Diagnostic report structure
 */
typedef struct nd_diagnostic_report {
    NDDiagnosticResult results[256];
    int count;
    int ok_count;
    int warning_count;
    int error_count;
    int critical_count;
    struct timeval start_time;
    struct timeval end_time;
} NDDiagnosticReport;

/**
 * Network test context structure
 */
typedef struct nd_test_context {
    NDServerInfo servers[ND_MAX_SERVERS];
    int server_count;
    NDDiagnosticReport *report;
    int test_flags;
    int verbose;
    int timeout_ms;
    int ping_count;
    long bandwidth_size;
} NDTestContext;

/* ============================================================
 * Server Management Functions
 * ============================================================ */

/**
 * Initialize server info structure
 * @param server Pointer to server info
 */
void nd_server_init(NDServerInfo *server);

/**
 * Set server information
 * @param server Pointer to server info
 * @param host Hostname or IP
 * @param port Port number
 * @param server_type Server type constant
 */
void nd_server_set(NDServerInfo *server, const char *host, int port, int server_type);

/**
 * Resolve server hostname to IP
 * @param server Pointer to server info
 * @return 0 on success, -1 on error
 */
int nd_server_resolve(NDServerInfo *server);

/**
 * Check if server is reachable
 * @param server Pointer to server info
 * @param timeout_ms Timeout in milliseconds
 * @return 1 if reachable, 0 otherwise
 */
int nd_server_is_reachable(NDServerInfo *server, int timeout_ms);

/* ============================================================
 * Connectivity Test Functions
 * ============================================================ */

/**
 * Test TCP connectivity to server
 * @param host Hostname or IP
 * @param port Port number
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -1 on error
 */
int nd_test_tcp_connect(const char *host, int port, int timeout_ms);

/**
 * Test UDP connectivity to server
 * @param host Hostname or IP
 * @param port Port number
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -1 on error
 */
int nd_test_udp_connect(const char *host, int port, int timeout_ms);

/**
 * Test if port is open
 * @param host Hostname or IP
 * @param port Port number
 * @param timeout_ms Timeout in milliseconds
 * @return 1 if open, 0 if closed, -1 on error
 */
int nd_test_port_open(const char *host, int port, int timeout_ms);

/**
 * Scan range of ports
 * @param host Hostname or IP
 * @param start_port Start port
 * @param end_port End port
 * @param results Array of port results
 * @param max_results Maximum results
 * @return Number of open ports found
 */
int nd_scan_ports(const char *host, int start_port, int end_port,
                  NDPortResult *results, int max_results);

/* ============================================================
 * Latency Test Functions
 * ============================================================ */

/**
 * Measure TCP connection latency
 * @param host Hostname or IP
 * @param port Port number
 * @param timeout_ms Timeout in milliseconds
 * @return Latency in milliseconds, -1 on error
 */
double nd_measure_tcp_latency(const char *host, int port, int timeout_ms);

/**
 * Run latency test with multiple samples
 * @param host Hostname or IP
 * @param port Port number
 * @param count Number of samples
 * @param timeout_ms Timeout in milliseconds
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int nd_run_latency_test(const char *host, int port, int count,
                        int timeout_ms, NDLatencyStats *stats);

/**
 * Calculate latency statistics
 * @param samples Array of latency samples
 * @param count Number of samples
 * @param stats Output statistics
 */
void nd_calculate_latency_stats(double *samples, int count, NDLatencyStats *stats);

/* ============================================================
 * Bandwidth Test Functions
 * ============================================================ */

/**
 * Test upload bandwidth
 * @param host Hostname or IP
 * @param port Port number
 * @param size_bytes Size of data to send
 * @param timeout_ms Timeout in milliseconds
 * @return Bandwidth in Mbps, -1 on error
 */
double nd_test_upload_bandwidth(const char *host, int port,
                                long size_bytes, int timeout_ms);

/**
 * Test download bandwidth
 * @param host Hostname or IP
 * @param port Port number
 * @param size_bytes Size of data to receive
 * @param timeout_ms Timeout in milliseconds
 * @return Bandwidth in Mbps, -1 on error
 */
double nd_test_download_bandwidth(const char *host, int port,
                                  long size_bytes, int timeout_ms);

/**
 * Run full bandwidth test
 * @param host Hostname or IP
 * @param port Port number
 * @param size_bytes Size of test data
 * @param timeout_ms Timeout in milliseconds
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int nd_run_bandwidth_test(const char *host, int port, long size_bytes,
                          int timeout_ms, NDBandwidthResult *result);

/* ============================================================
 * DNS Test Functions
 * ============================================================ */

/**
 * Resolve hostname to IP addresses
 * @param hostname Hostname to resolve
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int nd_resolve_hostname(const char *hostname, NDDnsResult *result);

/**
 * Reverse DNS lookup
 * @param ip IP address
 * @param hostname Output hostname buffer
 * @param hostname_size Buffer size
 * @return 0 on success, -1 on error
 */
int nd_reverse_dns(const char *ip, char *hostname, size_t hostname_size);

/**
 * Test DNS resolution time
 * @param hostname Hostname to resolve
 * @return Resolution time in milliseconds, -1 on error
 */
double nd_test_dns_resolution_time(const char *hostname);

/* ============================================================
 * MTU Discovery Functions
 * ============================================================ */

/**
 * Discover path MTU
 * @param host Hostname or IP
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int nd_discover_path_mtu(const char *host, NDMtuResult *result);

/**
 * Test if packet size works
 * @param host Hostname or IP
 * @param packet_size Packet size to test
 * @param timeout_ms Timeout in milliseconds
 * @return 1 if works, 0 if too large, -1 on error
 */
int nd_test_packet_size(const char *host, int packet_size, int timeout_ms);

/* ============================================================
 * Report Functions
 * ============================================================ */

/**
 * Initialize diagnostic report
 * @param report Pointer to report
 */
void nd_report_init(NDDiagnosticReport *report);

/**
 * Add result to report
 * @param report Pointer to report
 * @param level Severity level
 * @param test_type Test type
 * @param server Server hostname
 * @param port Port number
 * @param message Result message
 * @param suggestion Suggested action
 */
void nd_report_add(NDDiagnosticReport *report, int level, int test_type,
                   const char *server, int port, const char *message,
                   const char *suggestion);

/**
 * Print report to stdout
 * @param report Pointer to report
 * @param verbose Include detailed information
 */
void nd_report_print(NDDiagnosticReport *report, int verbose);

/**
 * Export report to JSON
 * @param report Pointer to report
 * @param filename Output filename
 * @return 0 on success, -1 on error
 */
int nd_report_export_json(NDDiagnosticReport *report, const char *filename);

/**
 * Export report to HTML
 * @param report Pointer to report
 * @param filename Output filename
 * @return 0 on success, -1 on error
 */
int nd_report_export_html(NDDiagnosticReport *report, const char *filename);

/**
 * Get report summary
 * @param report Pointer to report
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void nd_report_get_summary(NDDiagnosticReport *report, char *buffer, size_t buffer_size);

/* ============================================================
 * Test Context Functions
 * ============================================================ */

/**
 * Initialize test context
 * @param ctx Pointer to context
 * @param report Pointer to report
 */
void nd_context_init(NDTestContext *ctx, NDDiagnosticReport *report);

/**
 * Add server to context
 * @param ctx Pointer to context
 * @param host Hostname or IP
 * @param port Port number
 * @param server_type Server type
 * @return 0 on success, -1 if full
 */
int nd_context_add_server(NDTestContext *ctx, const char *host,
                          int port, int server_type);

/**
 * Load servers from config file
 * @param ctx Pointer to context
 * @param config_file Config file path
 * @return Number of servers loaded, -1 on error
 */
int nd_context_load_config(NDTestContext *ctx, const char *config_file);

/**
 * Set test flags
 * @param ctx Pointer to context
 * @param flags Test flags (ND_TEST_* constants)
 */
void nd_context_set_tests(NDTestContext *ctx, int flags);

/**
 * Set timeout
 * @param ctx Pointer to context
 * @param timeout_ms Timeout in milliseconds
 */
void nd_context_set_timeout(NDTestContext *ctx, int timeout_ms);

/**
 * Run all configured tests
 * @param ctx Pointer to context
 * @return Number of errors found
 */
int nd_context_run_tests(NDTestContext *ctx);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Get current time in milliseconds
 * @return Current time in milliseconds
 */
double nd_get_time_ms(void);

/**
 * Calculate time difference in milliseconds
 * @param start Start time
 * @param end End time
 * @return Difference in milliseconds
 */
double nd_time_diff_ms(struct timeval *start, struct timeval *end);

/**
 * Format bandwidth for display
 * @param mbps Bandwidth in Mbps
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void nd_format_bandwidth(double mbps, char *buffer, size_t buffer_size);

/**
 * Format latency for display
 * @param ms Latency in milliseconds
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void nd_format_latency(double ms, char *buffer, size_t buffer_size);

/**
 * Get level name string
 * @param level Severity level
 * @return Level name string
 */
const char *nd_get_level_name(int level);

/**
 * Get level color code (ANSI)
 * @param level Severity level
 * @return ANSI color code string
 */
const char *nd_get_level_color(int level);

/**
 * Get test type name
 * @param test_type Test type constant
 * @return Test type name string
 */
const char *nd_get_test_type_name(int test_type);

/**
 * Get server type name
 * @param server_type Server type constant
 * @return Server type name string
 */
const char *nd_get_server_type_name(int server_type);

/**
 * Check if IP address is valid
 * @param ip IP address string
 * @return 1 if valid, 0 otherwise
 */
int nd_is_valid_ip(const char *ip);

/**
 * Check if hostname is valid
 * @param hostname Hostname string
 * @return 1 if valid, 0 otherwise
 */
int nd_is_valid_hostname(const char *hostname);

/**
 * Parse host:port string
 * @param hostport Host:port string
 * @param host Output host buffer
 * @param host_size Host buffer size
 * @param port Output port
 * @return 0 on success, -1 on error
 */
int nd_parse_hostport(const char *hostport, char *host, size_t host_size, int *port);

#ifdef __cplusplus
}
#endif

#endif /* FDFS_NETWORK_DIAG_H */
