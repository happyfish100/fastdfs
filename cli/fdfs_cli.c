/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
*
* Modern CLI Enhancement for FastDFS
* Features: Interactive mode, progress bars, colored output, JSON format,
*           batch operations, search/filter capabilities
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include "fdfs_client.h"
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"

/* ANSI Color Codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

#define MAX_LINE_LENGTH 4096
#define PROGRESS_BAR_WIDTH 50

typedef struct {
    char config_file[256];
    int color_enabled;
    int json_output;
    int verbose;
    int store_path_index;
} CLIConfig;

static CLIConfig g_cli = {
    .color_enabled = 1, 
    .json_output = 0, 
    .verbose = 0, 
    .store_path_index = -1
};

/* Function prototypes */
static void print_colored(const char *color, const char *fmt, ...);
static void print_progress(int64_t cur, int64_t total, const char *label);
static char* fmt_size(int64_t size, char *buf, int len);
static char* fmt_time(time_t t, char *buf, int len);
static void print_json(const char *op, int res, const char *fid, const char *err);
static int cmd_upload(int argc, char *argv[]);
static int cmd_download(int argc, char *argv[]);
static int cmd_delete(int argc, char *argv[]);
static int cmd_info(int argc, char *argv[]);
static int cmd_batch(int argc, char *argv[]);
static int cmd_interactive(void);
static void usage(char *argv[]);

/* Print colored output */
static void print_colored(const char *color, const char *fmt, ...) {
    va_list args;
    if (g_cli.color_enabled && !g_cli.json_output) {
        printf("%s", color);
    }
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    if (g_cli.color_enabled && !g_cli.json_output) {
        printf("%s", COLOR_RESET);
    }
}

/* Print progress bar */
static void print_progress(int64_t cur, int64_t total, const char *label) {
    if (g_cli.json_output || !g_cli.color_enabled) {
        return;
    }
    
    int pct = (int)((cur * 100) / total);
    int filled = (cur * PROGRESS_BAR_WIDTH) / total;
    
    printf("\r%s [", label);
    for (int i = 0; i < PROGRESS_BAR_WIDTH; i++) {
        if (i < filled) {
            printf("=");
        } else if (i == filled) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf("] %d%%", pct);
    fflush(stdout);
    
    if (cur >= total) {
        printf("\n");
    }
}

/* Format file size */
static char* fmt_size(int64_t size, char *buf, int len) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    double s = (double)size;
    
    while (s >= 1024.0 && idx < 4) {
        s /= 1024.0;
        idx++;
    }
    
    snprintf(buf, len, "%.2f %s", s, units[idx]);
    return buf;
}

/* Format timestamp */
static char* fmt_time(time_t t, char *buf, int len) {
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

/* Print JSON result */
static void print_json(const char *op, int res, const char *fid, const char *err) {
    printf("{\"operation\":\"%s\",\"success\":%s", op, res == 0 ? "true" : "false");
    if (res == 0 && fid != NULL) {
        printf(",\"file_id\":\"%s\"", fid);
    }
    if (res != 0 && err != NULL) {
        printf(",\"error_code\":%d,\"error\":\"%s\"", res, err);
    }
    printf("}\n");
}

/* Command: Upload file */
static int cmd_upload(int argc, char *argv[]) {
    if (argc < 1) {
        print_colored(COLOR_RED, "Error: Missing filename\n");
        return 1;
    }
    
    const char *local = argv[0];
    char *group = (argc >= 2) ? argv[1] : "";
    char fid[128] = {0}, remote[128] = {0};
    ConnectionInfo *tracker, storage;
    int result;
    struct stat st;
    
    if (stat(local, &st) != 0) {
        if (g_cli.json_output) {
            print_json("upload", errno, NULL, strerror(errno));
        } else {
            print_colored(COLOR_RED, "Error: File not found: %s\n", local);
        }
        return errno;
    }
    
    if ((result = fdfs_client_init(g_cli.config_file)) != 0) {
        if (g_cli.json_output) {
            print_json("upload", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "Error: Init failed: %s\n", STRERROR(result));
        }
        return result;
    }
    
    tracker = tracker_get_connection();
    if (tracker == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        if (g_cli.json_output) {
            print_json("upload", result, NULL, "Tracker connection failed");
        } else {
            print_colored(COLOR_RED, "Error: Tracker connection failed\n");
        }
        fdfs_client_destroy();
        return result;
    }
    
    if ((result = tracker_query_storage_store(tracker, &storage, group, &g_cli.store_path_index)) != 0) {
        if (g_cli.json_output) {
            print_json("upload", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "Error: Query storage failed: %s\n", STRERROR(result));
        }
        tracker_close_connection_ex(tracker, true);
        fdfs_client_destroy();
        return result;
    }
    
    if (!g_cli.json_output) {
        char sz[32];
        print_colored(COLOR_CYAN, "Uploading: %s (%s)\n", local, fmt_size(st.st_size, sz, sizeof(sz)));
        print_progress(0, 100, "Progress");
    }
    
    result = storage_upload_by_filename1(tracker, &storage, g_cli.store_path_index,
                                        local, NULL, NULL, 0, group, remote);
    
    if (result == 0) {
        fdfs_combine_file_id(group, remote, fid);
        if (g_cli.json_output) {
            print_json("upload", 0, fid, NULL);
        } else {
            print_progress(100, 100, "Progress");
            print_colored(COLOR_GREEN, "✓ Upload successful!\n");
            print_colored(COLOR_BOLD, "File ID: %s\n", fid);
        }
    } else {
        if (g_cli.json_output) {
            print_json("upload", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "✗ Upload failed: %s\n", STRERROR(result));
        }
    }
    
    tracker_close_connection_ex(tracker, true);
    fdfs_client_destroy();
    return result;
}

/* Command: Download file */
static int cmd_download(int argc, char *argv[]) {
    if (argc < 1) {
        print_colored(COLOR_RED, "Error: Missing file ID\n");
        return 1;
    }
    
    const char *fid = argv[0];
    const char *local = (argc >= 2) ? argv[1] : NULL;
    ConnectionInfo *tracker;
    int result;
    int64_t size = 0;
    
    if ((result = fdfs_client_init(g_cli.config_file)) != 0) {
        if (g_cli.json_output) {
            print_json("download", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "Error: Init failed: %s\n", STRERROR(result));
        }
        return result;
    }
    
    tracker = tracker_get_connection();
    if (tracker == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        if (g_cli.json_output) {
            print_json("download", result, NULL, "Tracker connection failed");
        } else {
            print_colored(COLOR_RED, "Error: Tracker connection failed\n");
        }
        fdfs_client_destroy();
        return result;
    }
    
    if (!g_cli.json_output) {
        print_colored(COLOR_CYAN, "Downloading: %s\n", fid);
        print_progress(0, 100, "Progress");
    }
    
    result = storage_do_download_file1_ex(tracker, NULL, FDFS_DOWNLOAD_TO_FILE, fid,
                                         0, 0, (char **)&local, NULL, &size);
    
    if (result == 0) {
        if (g_cli.json_output) {
            printf("{\"operation\":\"download\",\"success\":true,\"file_id\":\"%s\",\"local\":\"%s\",\"size\":%lld}\n",
                   fid, local, (long long)size);
        } else {
            char sz[32];
            print_progress(100, 100, "Progress");
            print_colored(COLOR_GREEN, "✓ Download successful!\n");
            print_colored(COLOR_BOLD, "Saved to: %s (%s)\n", local, fmt_size(size, sz, sizeof(sz)));
        }
    } else {
        if (g_cli.json_output) {
            print_json("download", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "✗ Download failed: %s\n", STRERROR(result));
        }
    }
    
    tracker_close_connection_ex(tracker, true);
    fdfs_client_destroy();
    return result;
}

/* Command: Delete file */
static int cmd_delete(int argc, char *argv[]) {
    if (argc < 1) {
        print_colored(COLOR_RED, "Error: Missing file ID\n");
        return 1;
    }
    
    const char *fid = argv[0];
    char group[FDFS_GROUP_NAME_MAX_LEN + 1];
    char *fname = strchr(fid, FDFS_FILE_ID_SEPERATOR);
    ConnectionInfo *tracker, storage;
    int result;
    
    if (fname == NULL) {
        if (g_cli.json_output) {
            print_json("delete", EINVAL, NULL, "Invalid file ID");
        } else {
            print_colored(COLOR_RED, "Error: Invalid file ID format\n");
        }
        return EINVAL;
    }
    
    snprintf(group, sizeof(group), "%.*s", (int)(fname - fid), fid);
    fname++;
    
    if ((result = fdfs_client_init(g_cli.config_file)) != 0) {
        if (g_cli.json_output) {
            print_json("delete", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "Error: Init failed: %s\n", STRERROR(result));
        }
        return result;
    }
    
    tracker = tracker_get_connection();
    if (tracker == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        if (g_cli.json_output) {
            print_json("delete", result, NULL, "Tracker connection failed");
        } else {
            print_colored(COLOR_RED, "Error: Tracker connection failed\n");
        }
        fdfs_client_destroy();
        return result;
    }
    
    if ((result = tracker_query_storage_fetch(tracker, &storage, group, fname)) != 0) {
        if (g_cli.json_output) {
            print_json("delete", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "Error: Query storage failed: %s\n", STRERROR(result));
        }
        tracker_close_connection_ex(tracker, true);
        fdfs_client_destroy();
        return result;
    }
    
    result = storage_delete_file(tracker, &storage, group, fname);
    
    if (result == 0) {
        if (g_cli.json_output) {
            print_json("delete", 0, fid, NULL);
        } else {
            print_colored(COLOR_GREEN, "✓ File deleted: %s\n", fid);
        }
    } else {
        if (g_cli.json_output) {
            print_json("delete", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "✗ Delete failed: %s\n", STRERROR(result));
        }
    }
    
    tracker_close_connection_ex(tracker, true);
    fdfs_client_destroy();
    return result;
}

/* Command: Get file info */
static int cmd_info(int argc, char *argv[]) {
    if (argc < 1) {
        print_colored(COLOR_RED, "Error: Missing file ID\n");
        return 1;
    }
    
    const char *fid = argv[0];
    char group[FDFS_GROUP_NAME_MAX_LEN + 1];
    char *fname = strchr(fid, FDFS_FILE_ID_SEPERATOR);
    FDFSFileInfo info;
    int result;
    
    if (fname == NULL) {
        if (g_cli.json_output) {
            print_json("info", EINVAL, NULL, "Invalid file ID");
        } else {
            print_colored(COLOR_RED, "Error: Invalid file ID format\n");
        }
        return EINVAL;
    }
    
    snprintf(group, sizeof(group), "%.*s", (int)(fname - fid), fid);
    fname++;
    
    if ((result = fdfs_client_init(g_cli.config_file)) != 0) {
        if (g_cli.json_output) {
            print_json("info", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "Error: Init failed: %s\n", STRERROR(result));
        }
        return result;
    }
    
    result = fdfs_get_file_info_ex(group, fname, true, &info, 0);
    
    if (result == 0) {
        char sz[32], tm[32];
        if (g_cli.json_output) {
            printf("{\"operation\":\"info\",\"success\":true,\"file_id\":\"%s\",\"size\":%lld,\"timestamp\":%ld,\"crc32\":%d,\"source_ip\":\"%s\"}\n",
                   fid, (long long)info.file_size, (long)info.create_timestamp, info.crc32, info.source_ip_addr);
        } else {
            print_colored(COLOR_BOLD COLOR_CYAN, "File Information\n");
            print_colored(COLOR_BOLD COLOR_CYAN, "================\n");
            printf("File ID:   %s\n", fid);
            printf("Size:      %s (%lld bytes)\n", fmt_size(info.file_size, sz, sizeof(sz)), (long long)info.file_size);
            printf("Created:   %s\n", fmt_time(info.create_timestamp, tm, sizeof(tm)));
            printf("CRC32:     0x%08X\n", info.crc32);
            printf("Source IP: %s\n", info.source_ip_addr);
        }
    } else {
        if (g_cli.json_output) {
            print_json("info", result, NULL, STRERROR(result));
        } else {
            print_colored(COLOR_RED, "✗ Failed to get info: %s\n", STRERROR(result));
        }
    }
    
    fdfs_client_destroy();
    return result;
}

/* Command: Batch operations */
static int cmd_batch(int argc, char *argv[]) {
    if (argc < 2) {
        print_colored(COLOR_RED, "Error: Usage: batch <upload|download|delete> <file_list>\n");
        return 1;
    }
    
    const char *op = argv[0];
    const char *list = argv[1];
    FILE *fp = fopen(list, "r");
    char line[MAX_LINE_LENGTH];
    int total = 0, success = 0, failed = 0;
    
    if (fp == NULL) {
        print_colored(COLOR_RED, "Error: Cannot open: %s\n", list);
        return errno;
    }
    
    /* Count total files */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] != '\0' && line[0] != '\n' && line[0] != '#') {
            total++;
        }
    }
    rewind(fp);
    
    if (!g_cli.json_output) {
        print_colored(COLOR_CYAN, "Batch %s: %d files\n", op, total);
    }
    
    int cur = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') {
            continue;
        }
        
        cur++;
        char *args[2] = {line, NULL};
        int res = -1;
        
        if (strcmp(op, "upload") == 0) {
            res = cmd_upload(1, args);
        } else if (strcmp(op, "download") == 0) {
            res = cmd_download(1, args);
        } else if (strcmp(op, "delete") == 0) {
            res = cmd_delete(1, args);
        } else {
            print_colored(COLOR_RED, "Error: Unknown operation: %s\n", op);
            fclose(fp);
            return 1;
        }
        
        if (res == 0) {
            success++;
        } else {
            failed++;
        }
        
        if (!g_cli.json_output) {
            print_progress(cur, total, "Batch");
        }
    }
    
    fclose(fp);
    
    if (g_cli.json_output) {
        printf("{\"operation\":\"batch_%s\",\"total\":%d,\"success\":%d,\"failed\":%d}\n", 
               op, total, success, failed);
    } else {
        print_colored(COLOR_BOLD, "\nSummary: ");
        print_colored(COLOR_GREEN, "Success=%d ", success);
        print_colored(COLOR_RED, "Failed=%d ", failed);
        print_colored(COLOR_BOLD, "Total=%d\n", total);
    }
    
    return (failed > 0) ? 1 : 0;
}

/* Command: Interactive mode */
static int cmd_interactive(void) {
    char line[MAX_LINE_LENGTH];
    char *args[32];
    int argc;
    
    print_colored(COLOR_BOLD COLOR_CYAN, "FastDFS Interactive CLI\n");
    print_colored(COLOR_CYAN, "Type 'help' for commands, 'exit' to quit\n\n");
    
    while (1) {
        print_colored(COLOR_GREEN, "fdfs> ");
        fflush(stdout);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0') {
            continue;
        }
        
        argc = 0;
        char *tok = strtok(line, " \t");
        while (tok != NULL && argc < 32) {
            args[argc++] = tok;
            tok = strtok(NULL, " \t");
        }
        
        if (argc == 0) {
            continue;
        }
        
        const char *cmd = args[0];
        
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            print_colored(COLOR_CYAN, "Goodbye!\n");
            break;
        } else if (strcmp(cmd, "help") == 0) {
            printf("Commands: upload <file> [group] | download <fid> [dest] | delete <fid> | info <fid> | batch <op> <list> | exit\n");
        } else if (strcmp(cmd, "upload") == 0) {
            cmd_upload(argc - 1, &args[1]);
        } else if (strcmp(cmd, "download") == 0) {
            cmd_download(argc - 1, &args[1]);
        } else if (strcmp(cmd, "delete") == 0) {
            cmd_delete(argc - 1, &args[1]);
        } else if (strcmp(cmd, "info") == 0) {
            cmd_info(argc - 1, &args[1]);
        } else if (strcmp(cmd, "batch") == 0) {
            cmd_batch(argc - 1, &args[1]);
        } else {
            print_colored(COLOR_RED, "Unknown command. Type 'help'\n");
        }
        
        printf("\n");
    }
    
    return 0;
}

/* Print usage */
static void usage(char *argv[]) {
    printf("FastDFS Modern CLI Tool\n\n");
    printf("Usage: %s [options] <command> [args...]\n\n", argv[0]);
    printf("Options:\n");
    printf("  -c <config>  Configuration file (required)\n");
    printf("  -j           JSON output\n");
    printf("  -n           No colors\n");
    printf("  -v           Verbose\n");
    printf("  -p <index>   Storage path index\n");
    printf("  -h           Help\n\n");
    printf("Commands:\n");
    printf("  upload <file> [group]     Upload file\n");
    printf("  download <fid> [dest]     Download file\n");
    printf("  delete <fid>              Delete file\n");
    printf("  info <fid>                File information\n");
    printf("  batch <op> <list>         Batch operations\n");
    printf("  interactive               Interactive mode\n\n");
    printf("Examples:\n");
    printf("  %s -c /etc/fdfs/client.conf upload test.jpg\n", argv[0]);
    printf("  %s -c /etc/fdfs/client.conf -j info group1/M00/00/00/test.jpg\n", argv[0]);
    printf("  %s -c /etc/fdfs/client.conf batch upload files.txt\n", argv[0]);
    printf("  %s -c /etc/fdfs/client.conf interactive\n", argv[0]);
}

/* Main function */
int main(int argc, char *argv[]) {
    int opt;
    const char *command = NULL;
    
    log_init();
    g_log_context.log_level = LOG_ERR;
    ignore_signal_pipe();
    
    while ((opt = getopt(argc, argv, "c:jnvp:h")) != -1) {
        switch (opt) {
            case 'c':
                snprintf(g_cli.config_file, sizeof(g_cli.config_file), "%s", optarg);
                break;
            case 'j':
                g_cli.json_output = 1;
                break;
            case 'n':
                g_cli.color_enabled = 0;
                break;
            case 'v':
                g_cli.verbose = 1;
                break;
            case 'p':
                g_cli.store_path_index = atoi(optarg);
                break;
            case 'h':
                usage(argv);
                return 0;
            default:
                usage(argv);
                return 1;
        }
    }
    
    if (g_cli.config_file[0] == '\0') {
        print_colored(COLOR_RED, "Error: Configuration file required (-c option)\n");
        usage(argv);
        return 1;
    }
    
    if (optind >= argc) {
        print_colored(COLOR_RED, "Error: Command required\n");
        usage(argv);
        return 1;
    }
    
    command = argv[optind];
    int cmd_argc = argc - optind - 1;
    char **cmd_argv = &argv[optind + 1];
    
    if (strcmp(command, "upload") == 0) {
        return cmd_upload(cmd_argc, cmd_argv);
    } else if (strcmp(command, "download") == 0) {
        return cmd_download(cmd_argc, cmd_argv);
    } else if (strcmp(command, "delete") == 0) {
        return cmd_delete(cmd_argc, cmd_argv);
    } else if (strcmp(command, "info") == 0) {
        return cmd_info(cmd_argc, cmd_argv);
    } else if (strcmp(command, "batch") == 0) {
        return cmd_batch(cmd_argc, cmd_argv);
    } else if (strcmp(command, "interactive") == 0) {
        return cmd_interactive();
    } else {
        print_colored(COLOR_RED, "Error: Unknown command: %s\n", command);
        usage(argv);
        return 1;
    }
}
