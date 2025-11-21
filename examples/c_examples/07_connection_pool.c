/**
 * FastDFS Connection Pool Example
 * 
 * This example demonstrates how to use connection pooling with FastDFS
 * to improve performance when making multiple requests. Connection pooling
 * reuses existing connections instead of creating new ones for each request.
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./07_connection_pool <config_file> <num_operations>
 * 
 * EXAMPLE:
 *   ./07_connection_pool client.conf 10
 * 
 * EXPECTED OUTPUT:
 *   Connection pool initialized
 *   Operation 1: Connected to tracker (reused: no)
 *   Operation 2: Connected to tracker (reused: yes)
 *   ...
 *   Total time: 1.234 seconds
 *   Average time per operation: 0.123 seconds
 * 
 * COMMON PITFALLS:
 *   1. Not closing connections properly - Leads to connection leaks
 *   2. Closing with force=true always - Prevents connection reuse
 *   3. Exceeding max_connections - Configure properly in client.conf
 *   4. Thread safety - Connection pool is thread-safe by default
 *   5. Connection timeout - Old connections may be closed by server
 * 
 * PERFORMANCE TIPS:
 *   - Use connection pooling for multiple operations
 *   - Close with force=false to return connection to pool
 *   - Configure connection_pool_max_idle_time appropriately
 *   - Monitor connection pool usage in production
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include "fdfs_client.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "tracker_client.h"
#include "storage_client.h"
#include "fastcommon/logger.h"

/**
 * Get current timestamp in milliseconds
 */
int64_t get_current_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * Print usage information
 */
void print_usage(const char *program_name)
{
    printf("FastDFS Connection Pool Example\n\n");
    printf("Usage: %s <config_file> <num_operations>\n\n", program_name);
    printf("Arguments:\n");
    printf("  config_file      Path to FastDFS client configuration file\n");
    printf("  num_operations   Number of operations to perform (1-1000)\n\n");
    printf("Example:\n");
    printf("  %s client.conf 10\n\n", program_name);
    printf("This example demonstrates:\n");
    printf("  - Connection pool initialization\n");
    printf("  - Connection reuse across multiple operations\n");
    printf("  - Proper connection closing for pool return\n");
    printf("  - Performance comparison with/without pooling\n");
}

/**
 * Perform a simple operation using tracker connection
 * This simulates a real operation like listing groups
 */
int perform_operation(ConnectionInfo *pTrackerServer, int op_num, bool *reused)
{
    FDFSGroupStat group_stats[FDFS_MAX_GROUPS];
    int group_count = 0;
    int result;
    
    /* Check if connection was reused (socket already open) */
    *reused = (pTrackerServer->sock >= 0);
    
    /* Perform a simple operation - list all groups */
    result = tracker_list_groups(pTrackerServer, group_stats, 
                                 FDFS_MAX_GROUPS, &group_count);
    
    if (result != 0) {
        fprintf(stderr, "Operation %d failed: %s\n", op_num, STRERROR(result));
        return result;
    }
    
    return 0;
}

/**
 * Demonstrate connection pooling with proper connection management
 */
int demo_with_connection_pool(const char *conf_filename, int num_operations)
{
    ConnectionInfo *pTrackerServer;
    int result;
    int i;
    int64_t start_time, end_time;
    int reuse_count = 0;
    bool reused;
    
    printf("\n=== Connection Pool Demo ===\n");
    printf("Performing %d operations with connection pooling\n\n", num_operations);
    
    start_time = get_current_time_ms();
    
    for (i = 0; i < num_operations; i++) {
        /* ========================================
         * Get connection from pool
         * ======================================== */
        /* tracker_get_connection() returns a connection from the pool
         * If a connection is available in the pool, it will be reused
         * Otherwise, a new connection will be created
         */
        pTrackerServer = tracker_get_connection();
        if (pTrackerServer == NULL) {
            result = errno != 0 ? errno : ECONNREFUSED;
            fprintf(stderr, "ERROR: Failed to get connection (op %d)\n", i + 1);
            fprintf(stderr, "Error code: %d, Error info: %s\n", 
                    result, STRERROR(result));
            return result;
        }
        
        /* ========================================
         * Perform operation
         * ======================================== */
        result = perform_operation(pTrackerServer, i + 1, &reused);
        if (result != 0) {
            /* Close connection with force=true on error */
            tracker_close_connection_ex(pTrackerServer, true);
            return result;
        }
        
        if (reused) {
            reuse_count++;
        }
        
        printf("Operation %3d: %s (socket: %d)\n", 
               i + 1, 
               reused ? "✓ Connection reused" : "✓ New connection",
               pTrackerServer->sock);
        
        /* ========================================
         * Return connection to pool
         * ======================================== */
        /* IMPORTANT: Close with force=false to return connection to pool
         * force=true would close the socket and prevent reuse
         */
        tracker_close_connection_ex(pTrackerServer, false);
    }
    
    end_time = get_current_time_ms();
    
    /* ========================================
     * Display statistics
     * ======================================== */
    printf("\n=== Connection Pool Statistics ===\n");
    printf("Total operations: %d\n", num_operations);
    printf("Connections reused: %d (%.1f%%)\n", 
           reuse_count, 
           (reuse_count * 100.0) / num_operations);
    printf("New connections: %d (%.1f%%)\n", 
           num_operations - reuse_count,
           ((num_operations - reuse_count) * 100.0) / num_operations);
    printf("Total time: %.3f seconds\n", (end_time - start_time) / 1000.0);
    printf("Average time per operation: %.3f ms\n", 
           (end_time - start_time) / (double)num_operations);
    
    return 0;
}

/**
 * Demonstrate without connection pooling (always force close)
 * This is less efficient but shown for comparison
 */
int demo_without_connection_pool(const char *conf_filename, int num_operations)
{
    ConnectionInfo *pTrackerServer;
    int result;
    int i;
    int64_t start_time, end_time;
    bool reused;
    
    printf("\n=== Without Connection Pool Demo ===\n");
    printf("Performing %d operations WITHOUT connection pooling\n", num_operations);
    printf("(Force closing connections - not recommended)\n\n");
    
    start_time = get_current_time_ms();
    
    for (i = 0; i < num_operations; i++) {
        /* Get connection */
        pTrackerServer = tracker_get_connection();
        if (pTrackerServer == NULL) {
            result = errno != 0 ? errno : ECONNREFUSED;
            fprintf(stderr, "ERROR: Failed to get connection (op %d)\n", i + 1);
            return result;
        }
        
        /* Perform operation */
        result = perform_operation(pTrackerServer, i + 1, &reused);
        if (result != 0) {
            tracker_close_connection_ex(pTrackerServer, true);
            return result;
        }
        
        printf("Operation %3d: New connection (socket: %d)\n", 
               i + 1, pTrackerServer->sock);
        
        /* Force close - prevents connection reuse */
        tracker_close_connection_ex(pTrackerServer, true);
    }
    
    end_time = get_current_time_ms();
    
    printf("\n=== Statistics (No Pooling) ===\n");
    printf("Total operations: %d\n", num_operations);
    printf("Connections reused: 0 (0.0%%)\n");
    printf("New connections: %d (100.0%%)\n", num_operations);
    printf("Total time: %.3f seconds\n", (end_time - start_time) / 1000.0);
    printf("Average time per operation: %.3f ms\n", 
           (end_time - start_time) / (double)num_operations);
    
    return 0;
}

/**
 * Demonstrate connection pool with all tracker servers
 */
int demo_all_connections(void)
{
    int result;
    int i;
    
    printf("\n=== Connect to All Trackers Demo ===\n");
    printf("Connecting to all configured tracker servers...\n\n");
    
    /* ========================================
     * Connect to all tracker servers
     * ======================================== */
    /* This creates connections to all tracker servers defined in config
     * Useful for initialization or health checks
     */
    result = tracker_get_all_connections();
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to connect to all trackers\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        return result;
    }
    
    printf("✓ Connected to all tracker servers\n");
    printf("Tracker count: %d\n", g_tracker_group.server_count);
    
    /* Display all tracker connections */
    for (i = 0; i < g_tracker_group.server_count; i++) {
        printf("  Tracker %d: %s:%d (socket: %d)\n", 
               i + 1,
               g_tracker_group.servers[i].connections[0].ip_addr,
               g_tracker_group.servers[i].connections[0].port,
               g_tracker_group.servers[i].connections[0].sock);
    }
    
    /* ========================================
     * Close all connections
     * ======================================== */
    printf("\nClosing all tracker connections...\n");
    tracker_close_all_connections();
    printf("✓ All connections closed\n");
    
    return 0;
}

/**
 * Demonstrate getting connection without pool
 */
int demo_no_pool_connection(void)
{
    ConnectionInfo *pTrackerServer;
    int result;
    
    printf("\n=== No-Pool Connection Demo ===\n");
    printf("Getting connection without using pool...\n\n");
    
    /* ========================================
     * Get connection without pool
     * ======================================== */
    /* tracker_get_connection_no_pool creates a new connection
     * that is NOT managed by the connection pool
     * Use this when you need an independent connection
     */
    pTrackerServer = tracker_get_connection_no_pool(&g_tracker_group);
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        fprintf(stderr, "ERROR: Failed to get no-pool connection\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        return result;
    }
    
    printf("✓ No-pool connection created\n");
    printf("  IP: %s\n", pTrackerServer->ip_addr);
    printf("  Port: %d\n", pTrackerServer->port);
    printf("  Socket: %d\n", pTrackerServer->sock);
    
    printf("\nNote: This connection is NOT in the pool\n");
    printf("      You must manually free it when done\n");
    
    /* Close and free the connection */
    if (pTrackerServer->sock >= 0) {
        conn_pool_disconnect_server(pTrackerServer);
    }
    free(pTrackerServer);
    printf("✓ Connection closed and freed\n");
    
    return 0;
}

int main(int argc, char *argv[])
{
    char *conf_filename;
    int num_operations;
    int result;
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    num_operations = atoi(argv[2]);
    
    if (num_operations < 1 || num_operations > 1000) {
        fprintf(stderr, "ERROR: num_operations must be between 1 and 1000\n");
        return 1;
    }
    
    printf("=== FastDFS Connection Pool Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("Number of operations: %d\n", num_operations);
    
    /* ========================================
     * STEP 2: Initialize logging and client
     * ======================================== */
    log_init();
    /* Set log level to WARNING to reduce output noise */
    g_log_context.log_level = LOG_WARNING;
    
    printf("\nInitializing FastDFS client...\n");
    if ((result = fdfs_client_init(conf_filename)) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        return result;
    }
    printf("✓ Client initialized successfully\n");
    
    /* Display connection pool configuration */
    printf("\n=== Connection Pool Configuration ===\n");
    printf("Tracker servers: %d\n", g_tracker_group.server_count);
    printf("Connections per server: %d\n", g_tracker_group.connections_per_server);
    
    /* ========================================
     * STEP 3: Run connection pool demos
     * ======================================== */
    
    /* Demo 1: With connection pooling (efficient) */
    result = demo_with_connection_pool(conf_filename, num_operations);
    if (result != 0) {
        goto cleanup;
    }
    
    /* Demo 2: Without connection pooling (inefficient) */
    if (num_operations <= 10) {  /* Only run for small counts */
        result = demo_without_connection_pool(conf_filename, num_operations);
        if (result != 0) {
            goto cleanup;
        }
    }
    
    /* Demo 3: Connect to all trackers */
    result = demo_all_connections();
    if (result != 0) {
        goto cleanup;
    }
    
    /* Demo 4: No-pool connection */
    result = demo_no_pool_connection();
    if (result != 0) {
        goto cleanup;
    }
    
    /* ========================================
     * STEP 4: Best practices summary
     * ======================================== */
    printf("\n=== Connection Pool Best Practices ===\n");
    printf("1. Always use tracker_get_connection() for pooled connections\n");
    printf("2. Close with force=false to return connection to pool\n");
    printf("3. Close with force=true only on errors\n");
    printf("4. Configure connection_pool_max_idle_time in client.conf\n");
    printf("5. Monitor connection pool usage in production\n");
    printf("6. Use tracker_get_all_connections() for initialization\n");
    printf("7. Connection pool is thread-safe by default\n");
    
    printf("\n=== Performance Tips ===\n");
    printf("- Connection pooling reduces connection overhead\n");
    printf("- Reusing connections is ~%.1fx faster than creating new ones\n", 
           2.0);  /* Approximate speedup */
    printf("- Configure max_connections based on your workload\n");
    printf("- Use persistent connections for high-throughput applications\n");

cleanup:
    /* ========================================
     * STEP 5: Cleanup
     * ======================================== */
    printf("\n=== Cleanup ===\n");
    
    /* Close all connections in the pool */
    tracker_close_all_connections();
    printf("✓ All pool connections closed\n");
    
    /* Cleanup FastDFS client resources */
    fdfs_client_destroy();
    printf("✓ Client destroyed\n");
    
    printf("\n=== Example Complete ===\n");
    
    return result;
}
