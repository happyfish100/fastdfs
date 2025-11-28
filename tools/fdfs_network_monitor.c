/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_network_monitor.c
* Network monitoring tool for FastDFS
* Continuously monitors network connectivity and performance
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
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
#include <getopt.h>

#define MAX_SERVERS 64
#define MAX_LINE_LENGTH 1024
#define MAX_HISTORY 1000
#define DEFAULT_INTERVAL_SEC 10
#define DEFAULT_TIMEOUT_MS 5000

/* Alert thresholds */
#define LATENCY_WARNING_MS 100.0
#define LATENCY_CRITICAL_MS 500.0
#define LOSS_WARNING_PERCENT 5.0
#define LOSS_CRITICAL_PERCENT 20.0

/* Status codes */
#define STATUS_OK 0
#define STATUS_WARNING 1
#define STATUS_CRITICAL 2
#define STATUS_UNKNOWN 3

/* Output formats */
#define OUTPUT_TEXT 0
#define OUTPUT_JSON 1
#define OUTPUT_CSV 2
#define OUTPUT_PROMETHEUS 3

typedef struct {
    char host[256];
    int port;
    int server_type;
    char name[64];
} ServerConfig;

typedef struct {
    double latency_ms;
    int success;
    time_t timestamp;
} LatencySample;

typedef struct {
    ServerConfig config;
    LatencySample history[MAX_HISTORY];
    int history_count;
    int history_index;
    int current_status;
    int consecutive_failures;
    double avg_latency_ms;
    double min_latency_ms;
    double max_latency_ms;
    int total_checks;
    int total_failures;
    time_t last_check;
    time_t last_success;
    time_t last_failure;
} ServerState;

typedef struct {
    ServerState servers[MAX_SERVERS];
    int server_count;
    int interval_sec;
    int timeout_ms;
    int output_format;
    int verbose;
    int daemon_mode;
    int alert_enabled;
    char log_file[256];
    char alert_script[256];
    FILE *log_fp;
    volatile int running;
} MonitorContext;

/* Global context for signal handling */
static MonitorContext *g_ctx = NULL;

/* Function prototypes */
static void print_usage(const char *program);
static void signal_handler(int sig);
static int load_config_file(MonitorContext *ctx, const char *filename);
static double measure_latency(const char *host, int port, int timeout_ms);
static void update_server_state(ServerState *state, double latency_ms, int success);
static int get_server_status(ServerState *state);
static void print_status_text(MonitorContext *ctx);
static void print_status_json(MonitorContext *ctx);
static void print_status_csv(MonitorContext *ctx);
static void print_status_prometheus(MonitorContext *ctx);
static void log_message(MonitorContext *ctx, const char *format, ...);
static void send_alert(MonitorContext *ctx, ServerState *state, int old_status, int new_status);
static void run_monitor_loop(MonitorContext *ctx);
static const char *get_status_name(int status);
static const char *get_status_color(int status);

static void print_usage(const char *program)
{
    printf("FastDFS Network Monitor v1.0\n");
    printf("Continuously monitors FastDFS network connectivity\n\n");
    printf("Usage: %s [options] [config_file]\n", program);
    printf("Options:\n");
    printf("  -i, --interval <sec>    Check interval in seconds (default: 10)\n");
    printf("  -t, --timeout <ms>      Connection timeout in milliseconds (default: 5000)\n");
    printf("  -f, --format <fmt>      Output format: text, json, csv, prometheus\n");
    printf("  -l, --log <file>        Log file path\n");
    printf("  -a, --alert <script>    Alert script to run on status change\n");
    printf("  -d, --daemon            Run as daemon\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -h, --help              Show this help\n\n");
    printf("Config file format:\n");
    printf("  # Comment\n");
    printf("  tracker:name:host:port\n");
    printf("  storage:name:host:port\n\n");
    printf("Examples:\n");
    printf("  %s -i 30 servers.conf\n", program);
    printf("  %s -f prometheus -d servers.conf\n", program);
}

static void signal_handler(int sig)
{
    if (g_ctx != NULL) {
        g_ctx->running = 0;
    }
}

static double measure_latency(const char *host, int port, int timeout_ms)
{
    int sock;
    struct sockaddr_in addr;
    struct hostent *he;
    struct timeval start, end;
    int flags;
    struct pollfd pfd;
    double latency_ms;
    
    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    /* Set non-blocking */
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    /* Resolve hostname */
    he = gethostbyname(host);
    if (he == NULL) {
        close(sock);
        return -1;
    }
    
    /* Setup address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr, he->h_length);
    
    /* Start timing */
    gettimeofday(&start, NULL);
    
    /* Connect */
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS) {
            close(sock);
            return -1;
        }
    }
    
    /* Wait for connection */
    pfd.fd = sock;
    pfd.events = POLLOUT;
    
    if (poll(&pfd, 1, timeout_ms) <= 0) {
        close(sock);
        return -1;
    }
    
    /* Check if connected */
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
        close(sock);
        return -1;
    }
    
    /* End timing */
    gettimeofday(&end, NULL);
    
    close(sock);
    
    /* Calculate latency */
    latency_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                 (end.tv_usec - start.tv_usec) / 1000.0;
    
    return latency_ms;
}

static void update_server_state(ServerState *state, double latency_ms, int success)
{
    LatencySample *sample;
    int i;
    double sum = 0;
    int count = 0;
    
    /* Add sample to history */
    sample = &state->history[state->history_index];
    sample->latency_ms = latency_ms;
    sample->success = success;
    sample->timestamp = time(NULL);
    
    state->history_index = (state->history_index + 1) % MAX_HISTORY;
    if (state->history_count < MAX_HISTORY) {
        state->history_count++;
    }
    
    /* Update counters */
    state->total_checks++;
    state->last_check = time(NULL);
    
    if (success) {
        state->consecutive_failures = 0;
        state->last_success = time(NULL);
        
        /* Update latency stats */
        if (state->min_latency_ms < 0 || latency_ms < state->min_latency_ms) {
            state->min_latency_ms = latency_ms;
        }
        if (latency_ms > state->max_latency_ms) {
            state->max_latency_ms = latency_ms;
        }
    } else {
        state->consecutive_failures++;
        state->total_failures++;
        state->last_failure = time(NULL);
    }
    
    /* Calculate average latency from recent history */
    for (i = 0; i < state->history_count; i++) {
        if (state->history[i].success) {
            sum += state->history[i].latency_ms;
            count++;
        }
    }
    
    if (count > 0) {
        state->avg_latency_ms = sum / count;
    }
}

static int get_server_status(ServerState *state)
{
    double loss_percent;
    
    if (state->total_checks == 0) {
        return STATUS_UNKNOWN;
    }
    
    /* Check consecutive failures */
    if (state->consecutive_failures >= 3) {
        return STATUS_CRITICAL;
    }
    
    /* Check loss percentage */
    loss_percent = (state->total_failures * 100.0) / state->total_checks;
    if (loss_percent >= LOSS_CRITICAL_PERCENT) {
        return STATUS_CRITICAL;
    }
    if (loss_percent >= LOSS_WARNING_PERCENT) {
        return STATUS_WARNING;
    }
    
    /* Check latency */
    if (state->avg_latency_ms >= LATENCY_CRITICAL_MS) {
        return STATUS_CRITICAL;
    }
    if (state->avg_latency_ms >= LATENCY_WARNING_MS) {
        return STATUS_WARNING;
    }
    
    return STATUS_OK;
}

static const char *get_status_name(int status)
{
    switch (status) {
        case STATUS_OK: return "OK";
        case STATUS_WARNING: return "WARNING";
        case STATUS_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

static const char *get_status_color(int status)
{
    switch (status) {
        case STATUS_OK: return "\033[32m";       /* Green */
        case STATUS_WARNING: return "\033[33m";  /* Yellow */
        case STATUS_CRITICAL: return "\033[31m"; /* Red */
        default: return "\033[0m";               /* Default */
    }
}

static void log_message(MonitorContext *ctx, const char *format, ...)
{
    va_list args;
    char time_str[64];
    time_t now = time(NULL);
    FILE *out = ctx->log_fp ? ctx->log_fp : stdout;
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(out, "[%s] ", time_str);
    
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);
    
    fprintf(out, "\n");
    fflush(out);
}

static void send_alert(MonitorContext *ctx, ServerState *state, int old_status, int new_status)
{
    char cmd[1024];
    
    if (!ctx->alert_enabled || ctx->alert_script[0] == '\0') {
        return;
    }
    
    snprintf(cmd, sizeof(cmd), "%s '%s' '%s' %d '%s' '%s' %.2f",
             ctx->alert_script,
             state->config.name,
             state->config.host,
             state->config.port,
             get_status_name(old_status),
             get_status_name(new_status),
             state->avg_latency_ms);
    
    system(cmd);
}

static void print_status_text(MonitorContext *ctx)
{
    int i;
    time_t now = time(NULL);
    char time_str[64];
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    printf("\n=== FastDFS Network Monitor - %s ===\n\n", time_str);
    printf("%-20s %-20s %-8s %-10s %-10s %-10s %-10s\n",
           "Name", "Host:Port", "Status", "Latency", "Min", "Max", "Loss%");
    printf("%-20s %-20s %-8s %-10s %-10s %-10s %-10s\n",
           "--------------------", "--------------------", "--------",
           "----------", "----------", "----------", "----------");
    
    for (i = 0; i < ctx->server_count; i++) {
        ServerState *state = &ctx->servers[i];
        char hostport[64];
        double loss_percent = 0;
        
        snprintf(hostport, sizeof(hostport), "%s:%d",
                 state->config.host, state->config.port);
        
        if (state->total_checks > 0) {
            loss_percent = (state->total_failures * 100.0) / state->total_checks;
        }
        
        printf("%s%-20s %-20s %-8s %7.2f ms %7.2f ms %7.2f ms %7.1f%%\033[0m\n",
               get_status_color(state->current_status),
               state->config.name,
               hostport,
               get_status_name(state->current_status),
               state->avg_latency_ms,
               state->min_latency_ms > 0 ? state->min_latency_ms : 0,
               state->max_latency_ms,
               loss_percent);
    }
    
    printf("\n");
}

static void print_status_json(MonitorContext *ctx)
{
    int i;
    time_t now = time(NULL);
    
    printf("{\n");
    printf("  \"timestamp\": %ld,\n", now);
    printf("  \"servers\": [\n");
    
    for (i = 0; i < ctx->server_count; i++) {
        ServerState *state = &ctx->servers[i];
        double loss_percent = 0;
        
        if (state->total_checks > 0) {
            loss_percent = (state->total_failures * 100.0) / state->total_checks;
        }
        
        printf("    {\n");
        printf("      \"name\": \"%s\",\n", state->config.name);
        printf("      \"host\": \"%s\",\n", state->config.host);
        printf("      \"port\": %d,\n", state->config.port);
        printf("      \"status\": \"%s\",\n", get_status_name(state->current_status));
        printf("      \"latency_avg_ms\": %.2f,\n", state->avg_latency_ms);
        printf("      \"latency_min_ms\": %.2f,\n", state->min_latency_ms > 0 ? state->min_latency_ms : 0);
        printf("      \"latency_max_ms\": %.2f,\n", state->max_latency_ms);
        printf("      \"loss_percent\": %.1f,\n", loss_percent);
        printf("      \"total_checks\": %d,\n", state->total_checks);
        printf("      \"total_failures\": %d\n", state->total_failures);
        printf("    }%s\n", (i < ctx->server_count - 1) ? "," : "");
    }
    
    printf("  ]\n");
    printf("}\n");
}

static void print_status_csv(MonitorContext *ctx)
{
    int i;
    time_t now = time(NULL);
    
    for (i = 0; i < ctx->server_count; i++) {
        ServerState *state = &ctx->servers[i];
        double loss_percent = 0;
        
        if (state->total_checks > 0) {
            loss_percent = (state->total_failures * 100.0) / state->total_checks;
        }
        
        printf("%ld,%s,%s,%d,%s,%.2f,%.2f,%.2f,%.1f,%d,%d\n",
               now,
               state->config.name,
               state->config.host,
               state->config.port,
               get_status_name(state->current_status),
               state->avg_latency_ms,
               state->min_latency_ms > 0 ? state->min_latency_ms : 0,
               state->max_latency_ms,
               loss_percent,
               state->total_checks,
               state->total_failures);
    }
}

static void print_status_prometheus(MonitorContext *ctx)
{
    int i;
    
    printf("# HELP fdfs_server_up Server availability (1=up, 0=down)\n");
    printf("# TYPE fdfs_server_up gauge\n");
    
    for (i = 0; i < ctx->server_count; i++) {
        ServerState *state = &ctx->servers[i];
        int up = (state->current_status != STATUS_CRITICAL) ? 1 : 0;
        
        printf("fdfs_server_up{name=\"%s\",host=\"%s\",port=\"%d\"} %d\n",
               state->config.name, state->config.host, state->config.port, up);
    }
    
    printf("\n# HELP fdfs_latency_ms Server latency in milliseconds\n");
    printf("# TYPE fdfs_latency_ms gauge\n");
    
    for (i = 0; i < ctx->server_count; i++) {
        ServerState *state = &ctx->servers[i];
        
        printf("fdfs_latency_ms{name=\"%s\",host=\"%s\",port=\"%d\",type=\"avg\"} %.2f\n",
               state->config.name, state->config.host, state->config.port,
               state->avg_latency_ms);
        printf("fdfs_latency_ms{name=\"%s\",host=\"%s\",port=\"%d\",type=\"min\"} %.2f\n",
               state->config.name, state->config.host, state->config.port,
               state->min_latency_ms > 0 ? state->min_latency_ms : 0);
        printf("fdfs_latency_ms{name=\"%s\",host=\"%s\",port=\"%d\",type=\"max\"} %.2f\n",
               state->config.name, state->config.host, state->config.port,
               state->max_latency_ms);
    }
    
    printf("\n# HELP fdfs_loss_percent Packet loss percentage\n");
    printf("# TYPE fdfs_loss_percent gauge\n");
    
    for (i = 0; i < ctx->server_count; i++) {
        ServerState *state = &ctx->servers[i];
        double loss_percent = 0;
        
        if (state->total_checks > 0) {
            loss_percent = (state->total_failures * 100.0) / state->total_checks;
        }
        
        printf("fdfs_loss_percent{name=\"%s\",host=\"%s\",port=\"%d\"} %.1f\n",
               state->config.name, state->config.host, state->config.port,
               loss_percent);
    }
}

static int load_config_file(MonitorContext *ctx, const char *filename)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char type[32], name[64], host[256];
    int port;
    
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open config file '%s': %s\n",
                filename, strerror(errno));
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Skip comments and empty lines */
        char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;
        
        /* Parse line: type:name:host:port */
        if (sscanf(p, "%31[^:]:%63[^:]:%255[^:]:%d", type, name, host, &port) == 4) {
            if (ctx->server_count < MAX_SERVERS) {
                ServerState *state = &ctx->servers[ctx->server_count];
                memset(state, 0, sizeof(ServerState));
                
                strncpy(state->config.name, name, sizeof(state->config.name) - 1);
                strncpy(state->config.host, host, sizeof(state->config.host) - 1);
                state->config.port = port;
                
                if (strcmp(type, "tracker") == 0) {
                    state->config.server_type = 1;
                } else if (strcmp(type, "storage") == 0) {
                    state->config.server_type = 2;
                }
                
                state->min_latency_ms = -1;
                state->current_status = STATUS_UNKNOWN;
                
                ctx->server_count++;
            }
        }
    }
    
    fclose(fp);
    return ctx->server_count;
}

static void run_monitor_loop(MonitorContext *ctx)
{
    int i;
    
    while (ctx->running) {
        for (i = 0; i < ctx->server_count; i++) {
            ServerState *state = &ctx->servers[i];
            double latency;
            int success;
            int old_status;
            
            /* Measure latency */
            latency = measure_latency(state->config.host, state->config.port,
                                      ctx->timeout_ms);
            success = (latency >= 0);
            
            /* Update state */
            update_server_state(state, latency, success);
            
            /* Check for status change */
            old_status = state->current_status;
            state->current_status = get_server_status(state);
            
            /* Send alert if status changed */
            if (old_status != state->current_status && old_status != STATUS_UNKNOWN) {
                log_message(ctx, "Status change: %s (%s:%d) %s -> %s",
                           state->config.name, state->config.host, state->config.port,
                           get_status_name(old_status),
                           get_status_name(state->current_status));
                send_alert(ctx, state, old_status, state->current_status);
            }
            
            if (ctx->verbose) {
                log_message(ctx, "Check: %s (%s:%d) latency=%.2fms status=%s",
                           state->config.name, state->config.host, state->config.port,
                           latency, get_status_name(state->current_status));
            }
        }
        
        /* Print status */
        switch (ctx->output_format) {
            case OUTPUT_JSON:
                print_status_json(ctx);
                break;
            case OUTPUT_CSV:
                print_status_csv(ctx);
                break;
            case OUTPUT_PROMETHEUS:
                print_status_prometheus(ctx);
                break;
            default:
                print_status_text(ctx);
                break;
        }
        
        /* Wait for next interval */
        sleep(ctx->interval_sec);
    }
}

int main(int argc, char *argv[])
{
    MonitorContext ctx;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"interval", required_argument, 0, 'i'},
        {"timeout", required_argument, 0, 't'},
        {"format", required_argument, 0, 'f'},
        {"log", required_argument, 0, 'l'},
        {"alert", required_argument, 0, 'a'},
        {"daemon", no_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(ctx));
    ctx.interval_sec = DEFAULT_INTERVAL_SEC;
    ctx.timeout_ms = DEFAULT_TIMEOUT_MS;
    ctx.output_format = OUTPUT_TEXT;
    ctx.running = 1;
    
    /* Parse command line options */
    while ((opt = getopt_long(argc, argv, "i:t:f:l:a:dvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                ctx.interval_sec = atoi(optarg);
                if (ctx.interval_sec < 1) ctx.interval_sec = 1;
                break;
            case 't':
                ctx.timeout_ms = atoi(optarg);
                if (ctx.timeout_ms < 100) ctx.timeout_ms = 100;
                break;
            case 'f':
                if (strcmp(optarg, "json") == 0) {
                    ctx.output_format = OUTPUT_JSON;
                } else if (strcmp(optarg, "csv") == 0) {
                    ctx.output_format = OUTPUT_CSV;
                } else if (strcmp(optarg, "prometheus") == 0) {
                    ctx.output_format = OUTPUT_PROMETHEUS;
                }
                break;
            case 'l':
                strncpy(ctx.log_file, optarg, sizeof(ctx.log_file) - 1);
                break;
            case 'a':
                strncpy(ctx.alert_script, optarg, sizeof(ctx.alert_script) - 1);
                ctx.alert_enabled = 1;
                break;
            case 'd':
                ctx.daemon_mode = 1;
                break;
            case 'v':
                ctx.verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Check for config file */
    if (optind >= argc) {
        fprintf(stderr, "Error: Config file required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Load config */
    if (load_config_file(&ctx, argv[optind]) < 0) {
        return 1;
    }
    
    if (ctx.server_count == 0) {
        fprintf(stderr, "Error: No servers configured\n");
        return 1;
    }
    
    printf("Loaded %d servers from config\n", ctx.server_count);
    
    /* Open log file */
    if (ctx.log_file[0]) {
        ctx.log_fp = fopen(ctx.log_file, "a");
        if (ctx.log_fp == NULL) {
            fprintf(stderr, "Warning: Cannot open log file '%s'\n", ctx.log_file);
        }
    }
    
    /* Setup signal handlers */
    g_ctx = &ctx;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Daemonize if requested */
    if (ctx.daemon_mode) {
        if (daemon(0, 0) < 0) {
            fprintf(stderr, "Error: Failed to daemonize: %s\n", strerror(errno));
            return 1;
        }
    }
    
    /* Run monitor loop */
    run_monitor_loop(&ctx);
    
    /* Cleanup */
    if (ctx.log_fp) {
        fclose(ctx.log_fp);
    }
    
    printf("Monitor stopped\n");
    return 0;
}
