/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_network_diag.c
* Network diagnostics tool for FastDFS
* Diagnoses network connectivity and performance between tracker and storage servers
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>

#define MAX_SERVERS 64
#define MAX_LINE_LENGTH 1024
#define DEFAULT_TRACKER_PORT 22122
#define DEFAULT_STORAGE_PORT 23000
#define DEFAULT_TIMEOUT_MS 5000
#define PING_COUNT 5
#define BANDWIDTH_TEST_SIZE (1024 * 1024)  /* 1MB */

#define DIAG_OK 0
#define DIAG_WARNING 1
#define DIAG_ERROR 2

typedef struct {
    char host[256];
    int port;
    int is_tracker;
} ServerInfo;

typedef struct {
    double min_latency_ms;
    double max_latency_ms;
    double avg_latency_ms;
    int success_count;
    int fail_count;
    int connection_refused;
    int timeout_count;
} LatencyResult;

typedef struct {
    double bandwidth_mbps;
    int test_success;
    char error_msg[256];
} BandwidthResult;

typedef struct {
    ServerInfo server;
    LatencyResult latency;
    BandwidthResult bandwidth;
    int tcp_nodelay_supported;
    int keepalive_supported;
    int overall_status;
} DiagResult;

/* Function prototypes */
static void print_usage(const char *program);
static int parse_server_address(const char *addr, ServerInfo *server);
static int load_servers_from_config(const char *config_file, ServerInfo *servers, int *count, int is_tracker);
static double get_time_ms(void);
static int test_tcp_connection(const char *host, int port, int timeout_ms, double *latency_ms);
static void test_latency(ServerInfo *server, LatencyResult *result, int count, int timeout_ms);
static void test_bandwidth(ServerInfo *server, BandwidthResult *result);
static void test_tcp_options(ServerInfo *server, DiagResult *result);
static void run_diagnostics(ServerInfo *server, DiagResult *result, int verbose);
static void print_result(DiagResult *result);
static void print_summary(DiagResult *results, int count);
static const char *status_to_string(int status);
static const char *status_to_color(int status);

static void print_usage(const char *program)
{
    printf("FastDFS Network Diagnostics Tool v1.0\n");
    printf("Diagnoses network connectivity and performance issues\n\n");
    printf("Usage: %s [options] <server_address> [server_address...]\n", program);
    printf("       %s [options] -c <config_file>\n\n", program);
    printf("Options:\n");
    printf("  -c <file>   Load servers from config file (tracker.conf or storage.conf)\n");
    printf("  -t          Test as tracker server (default port: 22122)\n");
    printf("  -s          Test as storage server (default port: 23000)\n");
    printf("  -p <port>   Specify port number\n");
    printf("  -n <count>  Number of ping tests (default: 5)\n");
    printf("  -T <ms>     Connection timeout in milliseconds (default: 5000)\n");
    printf("  -b          Run bandwidth test\n");
    printf("  -v          Verbose output\n");
    printf("  -h          Show this help\n\n");
    printf("Server address format: host[:port]\n\n");
    printf("Examples:\n");
    printf("  %s 192.168.1.100:22122\n", program);
    printf("  %s -t 192.168.1.100 192.168.1.101\n", program);
    printf("  %s -c /etc/fdfs/storage.conf\n", program);
    printf("  %s -b -n 10 192.168.1.100:23000\n", program);
}

static double get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static int parse_server_address(const char *addr, ServerInfo *server)
{
    char *colon;
    char temp[256];
    
    strncpy(temp, addr, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    colon = strchr(temp, ':');
    if (colon != NULL) {
        *colon = '\0';
        server->port = atoi(colon + 1);
        if (server->port <= 0 || server->port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", colon + 1);
            return -1;
        }
    } else {
        server->port = server->is_tracker ? DEFAULT_TRACKER_PORT : DEFAULT_STORAGE_PORT;
    }
    
    strncpy(server->host, temp, sizeof(server->host) - 1);
    return 0;
}

static int load_servers_from_config(const char *config_file, ServerInfo *servers, int *count, int is_tracker)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *key, *value, *eq_pos;
    const char *target_key = "tracker_server";
    
    *count = 0;
    
    fp = fopen(config_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "Cannot open config file: %s\n", config_file);
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL && *count < MAX_SERVERS) {
        /* Skip comments */
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n') {
            continue;
        }
        
        eq_pos = strchr(trimmed, '=');
        if (eq_pos == NULL) continue;
        
        *eq_pos = '\0';
        key = trimmed;
        value = eq_pos + 1;
        
        /* Trim whitespace */
        while (*key && (*key == ' ' || *key == '\t')) key++;
        char *end = key + strlen(key) - 1;
        while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';
        
        while (*value && (*value == ' ' || *value == '\t')) value++;
        end = value + strlen(value) - 1;
        while (end > value && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
        
        if (strcmp(key, target_key) == 0) {
            servers[*count].is_tracker = 1;
            if (parse_server_address(value, &servers[*count]) == 0) {
                (*count)++;
            }
        }
    }
    
    fclose(fp);
    return *count > 0 ? 0 : -1;
}

static int test_tcp_connection(const char *host, int port, int timeout_ms, double *latency_ms)
{
    int sock;
    struct sockaddr_in addr;
    struct hostent *he;
    double start_time, end_time;
    int flags, result;
    struct pollfd pfd;
    int error = 0;
    socklen_t len = sizeof(error);
    
    *latency_ms = -1;
    
    /* Resolve hostname */
    he = gethostbyname(host);
    if (he == NULL) {
        return -1;
    }
    
    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -2;
    }
    
    /* Set non-blocking */
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    /* Setup address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    /* Start timing */
    start_time = get_time_ms();
    
    /* Connect */
    result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (result < 0 && errno != EINPROGRESS) {
        close(sock);
        return -3;  /* Connection refused */
    }
    
    /* Wait for connection */
    pfd.fd = sock;
    pfd.events = POLLOUT;
    result = poll(&pfd, 1, timeout_ms);
    
    end_time = get_time_ms();
    
    if (result <= 0) {
        close(sock);
        return -4;  /* Timeout */
    }
    
    /* Check if connection succeeded */
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
    close(sock);
    
    if (error != 0) {
        return -3;  /* Connection refused */
    }
    
    *latency_ms = end_time - start_time;
    return 0;
}

static void test_latency(ServerInfo *server, LatencyResult *result, int count, int timeout_ms)
{
    int i;
    double latency;
    int ret;
    double total_latency = 0;
    
    memset(result, 0, sizeof(LatencyResult));
    result->min_latency_ms = 999999;
    result->max_latency_ms = 0;
    
    for (i = 0; i < count; i++) {
        ret = test_tcp_connection(server->host, server->port, timeout_ms, &latency);
        
        if (ret == 0) {
            result->success_count++;
            total_latency += latency;
            
            if (latency < result->min_latency_ms) {
                result->min_latency_ms = latency;
            }
            if (latency > result->max_latency_ms) {
                result->max_latency_ms = latency;
            }
        } else if (ret == -3) {
            result->connection_refused++;
            result->fail_count++;
        } else if (ret == -4) {
            result->timeout_count++;
            result->fail_count++;
        } else {
            result->fail_count++;
        }
        
        /* Small delay between tests */
        if (i < count - 1) {
            usleep(100000);  /* 100ms */
        }
    }
    
    if (result->success_count > 0) {
        result->avg_latency_ms = total_latency / result->success_count;
    }
    
    if (result->min_latency_ms == 999999) {
        result->min_latency_ms = 0;
    }
}

static void test_bandwidth(ServerInfo *server, BandwidthResult *result)
{
    int sock;
    struct sockaddr_in addr;
    struct hostent *he;
    char *buffer;
    double start_time, end_time;
    ssize_t sent, total_sent = 0;
    double duration;
    
    memset(result, 0, sizeof(BandwidthResult));
    
    /* Allocate test buffer */
    buffer = (char *)malloc(BANDWIDTH_TEST_SIZE);
    if (buffer == NULL) {
        snprintf(result->error_msg, sizeof(result->error_msg), "Memory allocation failed");
        return;
    }
    memset(buffer, 'A', BANDWIDTH_TEST_SIZE);
    
    /* Resolve hostname */
    he = gethostbyname(server->host);
    if (he == NULL) {
        snprintf(result->error_msg, sizeof(result->error_msg), "Cannot resolve hostname");
        free(buffer);
        return;
    }
    
    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        snprintf(result->error_msg, sizeof(result->error_msg), "Socket creation failed");
        free(buffer);
        return;
    }
    
    /* Setup address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    /* Connect */
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        snprintf(result->error_msg, sizeof(result->error_msg), "Connection failed: %s", strerror(errno));
        close(sock);
        free(buffer);
        return;
    }
    
    /* Send data and measure time */
    start_time = get_time_ms();
    
    while (total_sent < BANDWIDTH_TEST_SIZE) {
        sent = send(sock, buffer + total_sent, BANDWIDTH_TEST_SIZE - total_sent, 0);
        if (sent <= 0) {
            break;
        }
        total_sent += sent;
    }
    
    end_time = get_time_ms();
    
    close(sock);
    free(buffer);
    
    if (total_sent > 0) {
        duration = (end_time - start_time) / 1000.0;  /* Convert to seconds */
        if (duration > 0) {
            result->bandwidth_mbps = (total_sent * 8.0) / (duration * 1000000.0);
            result->test_success = 1;
        }
    } else {
        snprintf(result->error_msg, sizeof(result->error_msg), "No data sent");
    }
}

static void test_tcp_options(ServerInfo *server, DiagResult *result)
{
    int sock;
    struct sockaddr_in addr;
    struct hostent *he;
    int flag = 1;
    int ret;
    
    result->tcp_nodelay_supported = 0;
    result->keepalive_supported = 0;
    
    he = gethostbyname(server->host);
    if (he == NULL) return;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return;
    }
    
    /* Test TCP_NODELAY */
    ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    result->tcp_nodelay_supported = (ret == 0);
    
    /* Test SO_KEEPALIVE */
    ret = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
    result->keepalive_supported = (ret == 0);
    
    close(sock);
}

static const char *status_to_string(int status)
{
    switch (status) {
        case DIAG_OK: return "OK";
        case DIAG_WARNING: return "WARNING";
        case DIAG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static const char *status_to_color(int status)
{
    switch (status) {
        case DIAG_OK: return "\033[32m";      /* Green */
        case DIAG_WARNING: return "\033[33m"; /* Yellow */
        case DIAG_ERROR: return "\033[31m";   /* Red */
        default: return "\033[0m";
    }
}

static void run_diagnostics(ServerInfo *server, DiagResult *result, int verbose)
{
    memset(result, 0, sizeof(DiagResult));
    memcpy(&result->server, server, sizeof(ServerInfo));
    
    if (verbose) {
        printf("Testing %s:%d...\n", server->host, server->port);
    }
    
    /* Test latency */
    test_latency(server, &result->latency, PING_COUNT, DEFAULT_TIMEOUT_MS);
    
    /* Test TCP options if connection succeeded */
    if (result->latency.success_count > 0) {
        test_tcp_options(server, result);
    }
    
    /* Determine overall status */
    if (result->latency.success_count == 0) {
        result->overall_status = DIAG_ERROR;
    } else if (result->latency.fail_count > 0 || result->latency.avg_latency_ms > 100) {
        result->overall_status = DIAG_WARNING;
    } else {
        result->overall_status = DIAG_OK;
    }
}

static void print_result(DiagResult *result)
{
    const char *color = status_to_color(result->overall_status);
    
    printf("\n");
    printf("========================================\n");
    printf("Server: %s:%d\n", result->server.host, result->server.port);
    printf("Type: %s\n", result->server.is_tracker ? "Tracker" : "Storage");
    printf("========================================\n");
    
    /* Connection status */
    printf("\n%s[%s]\033[0m Connection Status\n", color, status_to_string(result->overall_status));
    
    if (result->latency.success_count == 0) {
        printf("  \033[31mFailed to connect!\033[0m\n");
        if (result->latency.connection_refused > 0) {
            printf("  - Connection refused (server not running or firewall blocking)\n");
        }
        if (result->latency.timeout_count > 0) {
            printf("  - Connection timeout (network issue or server overloaded)\n");
        }
    } else {
        printf("  Success: %d/%d connections\n", 
            result->latency.success_count, 
            result->latency.success_count + result->latency.fail_count);
    }
    
    /* Latency */
    if (result->latency.success_count > 0) {
        printf("\nLatency:\n");
        printf("  Min: %.2f ms\n", result->latency.min_latency_ms);
        printf("  Max: %.2f ms\n", result->latency.max_latency_ms);
        printf("  Avg: %.2f ms\n", result->latency.avg_latency_ms);
        
        if (result->latency.avg_latency_ms < 1) {
            printf("  \033[32m[Excellent] Sub-millisecond latency\033[0m\n");
        } else if (result->latency.avg_latency_ms < 10) {
            printf("  \033[32m[Good] Low latency\033[0m\n");
        } else if (result->latency.avg_latency_ms < 50) {
            printf("  \033[33m[OK] Moderate latency\033[0m\n");
        } else if (result->latency.avg_latency_ms < 100) {
            printf("  \033[33m[Warning] High latency - may affect performance\033[0m\n");
        } else {
            printf("  \033[31m[Critical] Very high latency - will impact performance\033[0m\n");
        }
    }
    
    /* TCP Options */
    if (result->latency.success_count > 0) {
        printf("\nTCP Options:\n");
        printf("  TCP_NODELAY: %s\n", result->tcp_nodelay_supported ? "Supported" : "Not supported");
        printf("  SO_KEEPALIVE: %s\n", result->keepalive_supported ? "Supported" : "Not supported");
    }
    
    /* Bandwidth */
    if (result->bandwidth.test_success) {
        printf("\nBandwidth:\n");
        printf("  Upload: %.2f Mbps\n", result->bandwidth.bandwidth_mbps);
        
        if (result->bandwidth.bandwidth_mbps > 1000) {
            printf("  \033[32m[Excellent] Gigabit+ speed\033[0m\n");
        } else if (result->bandwidth.bandwidth_mbps > 100) {
            printf("  \033[32m[Good] Fast network\033[0m\n");
        } else if (result->bandwidth.bandwidth_mbps > 10) {
            printf("  \033[33m[OK] Moderate speed\033[0m\n");
        } else {
            printf("  \033[31m[Warning] Slow network\033[0m\n");
        }
    } else if (result->bandwidth.error_msg[0] != '\0') {
        printf("\nBandwidth: Test failed - %s\n", result->bandwidth.error_msg);
    }
    
    /* Recommendations */
    printf("\nRecommendations:\n");
    if (result->latency.success_count == 0) {
        printf("  1. Check if the server is running\n");
        printf("  2. Verify firewall rules allow connections to port %d\n", result->server.port);
        printf("  3. Check network connectivity (ping, traceroute)\n");
    } else {
        if (result->latency.avg_latency_ms > 50) {
            printf("  - Consider placing servers in same datacenter/network\n");
        }
        if (result->latency.fail_count > 0) {
            printf("  - Investigate intermittent connection failures\n");
        }
        if (result->latency.max_latency_ms > result->latency.avg_latency_ms * 3) {
            printf("  - High latency variance detected - check for network congestion\n");
        }
        if (result->overall_status == DIAG_OK) {
            printf("  - Network looks healthy!\n");
        }
    }
}

static void print_summary(DiagResult *results, int count)
{
    int i;
    int ok_count = 0, warn_count = 0, error_count = 0;
    
    printf("\n");
    printf("========================================\n");
    printf("Summary\n");
    printf("========================================\n\n");
    
    for (i = 0; i < count; i++) {
        const char *color = status_to_color(results[i].overall_status);
        printf("%s[%s]\033[0m %s:%d", 
            color,
            status_to_string(results[i].overall_status),
            results[i].server.host,
            results[i].server.port);
        
        if (results[i].latency.success_count > 0) {
            printf(" - %.2f ms avg", results[i].latency.avg_latency_ms);
        }
        printf("\n");
        
        switch (results[i].overall_status) {
            case DIAG_OK: ok_count++; break;
            case DIAG_WARNING: warn_count++; break;
            case DIAG_ERROR: error_count++; break;
        }
    }
    
    printf("\nTotal: %d OK, %d Warnings, %d Errors\n", ok_count, warn_count, error_count);
    
    if (error_count > 0) {
        printf("\n\033[31mSome servers have connectivity issues!\033[0m\n");
    } else if (warn_count > 0) {
        printf("\n\033[33mSome servers have performance warnings.\033[0m\n");
    } else {
        printf("\n\033[32mAll servers are healthy!\033[0m\n");
    }
}

int main(int argc, char *argv[])
{
    int opt;
    int is_tracker = 0;
    int default_port = 0;
    int ping_count = PING_COUNT;
    int timeout_ms = DEFAULT_TIMEOUT_MS;
    int run_bandwidth = 0;
    int verbose = 0;
    const char *config_file = NULL;
    ServerInfo servers[MAX_SERVERS];
    DiagResult results[MAX_SERVERS];
    int server_count = 0;
    int i;
    
    while ((opt = getopt(argc, argv, "c:tsp:n:T:bvh")) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 't':
                is_tracker = 1;
                if (default_port == 0) default_port = DEFAULT_TRACKER_PORT;
                break;
            case 's':
                is_tracker = 0;
                if (default_port == 0) default_port = DEFAULT_STORAGE_PORT;
                break;
            case 'p':
                default_port = atoi(optarg);
                break;
            case 'n':
                ping_count = atoi(optarg);
                if (ping_count < 1) ping_count = 1;
                if (ping_count > 100) ping_count = 100;
                break;
            case 'T':
                timeout_ms = atoi(optarg);
                if (timeout_ms < 100) timeout_ms = 100;
                break;
            case 'b':
                run_bandwidth = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }
    
    /* Load servers from config file */
    if (config_file != NULL) {
        if (load_servers_from_config(config_file, servers, &server_count, is_tracker) != 0) {
            fprintf(stderr, "No servers found in config file\n");
            return 1;
        }
        printf("Loaded %d servers from %s\n", server_count, config_file);
    }
    
    /* Add servers from command line */
    for (i = optind; i < argc && server_count < MAX_SERVERS; i++) {
        servers[server_count].is_tracker = is_tracker;
        if (default_port > 0) {
            servers[server_count].port = default_port;
        }
        if (parse_server_address(argv[i], &servers[server_count]) == 0) {
            server_count++;
        }
    }
    
    if (server_count == 0) {
        fprintf(stderr, "No servers specified\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    printf("FastDFS Network Diagnostics\n");
    printf("Testing %d server(s)...\n", server_count);
    
    /* Run diagnostics on each server */
    for (i = 0; i < server_count; i++) {
        run_diagnostics(&servers[i], &results[i], verbose);
        
        if (run_bandwidth && results[i].latency.success_count > 0) {
            if (verbose) {
                printf("Running bandwidth test for %s:%d...\n", 
                    servers[i].host, servers[i].port);
            }
            test_bandwidth(&servers[i], &results[i].bandwidth);
        }
        
        print_result(&results[i]);
    }
    
    /* Print summary if multiple servers */
    if (server_count > 1) {
        print_summary(results, server_count);
    }
    
    /* Return error code if any server failed */
    for (i = 0; i < server_count; i++) {
        if (results[i].overall_status == DIAG_ERROR) {
            return 1;
        }
    }
    
    return 0;
}
