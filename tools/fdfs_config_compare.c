/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_config_compare.c
* Configuration comparison tool for FastDFS
* Compares two configuration files and reports differences
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>

#define MAX_LINE_LENGTH 1024
#define MAX_PATH_LENGTH 256
#define MAX_CONFIG_ITEMS 200
#define MAX_DIFF_ITEMS 100

/* Difference types */
#define DIFF_ADDED 1
#define DIFF_REMOVED 2
#define DIFF_MODIFIED 3
#define DIFF_UNCHANGED 0

/* Output formats */
#define OUTPUT_TEXT 0
#define OUTPUT_JSON 1
#define OUTPUT_HTML 2

typedef struct {
    char key[64];
    char value[256];
    int line_number;
} ConfigItem;

typedef struct {
    ConfigItem items[MAX_CONFIG_ITEMS];
    int count;
    char filename[MAX_PATH_LENGTH];
    time_t modified_time;
} ConfigFile;

typedef struct {
    char key[64];
    char value1[256];
    char value2[256];
    int diff_type;
    int line1;
    int line2;
} DiffItem;

typedef struct {
    DiffItem items[MAX_DIFF_ITEMS];
    int count;
    int added;
    int removed;
    int modified;
    int unchanged;
} DiffReport;

typedef struct {
    int output_format;
    int show_unchanged;
    int ignore_comments;
    int ignore_whitespace;
    int verbose;
    char output_file[MAX_PATH_LENGTH];
} CompareOptions;

/* Function prototypes */
static void print_usage(const char *program);
static int load_config_file(const char *filename, ConfigFile *config);
static char *trim_string(char *str);
static const char *get_config_value(ConfigFile *config, const char *key);
static int find_config_item(ConfigFile *config, const char *key);
static void compare_configs(ConfigFile *config1, ConfigFile *config2, 
                           DiffReport *report, CompareOptions *options);
static void print_diff_report_text(DiffReport *report, ConfigFile *config1, 
                                   ConfigFile *config2, CompareOptions *options);
static void print_diff_report_json(DiffReport *report, ConfigFile *config1, 
                                   ConfigFile *config2, CompareOptions *options);
static void print_diff_report_html(DiffReport *report, ConfigFile *config1, 
                                   ConfigFile *config2, CompareOptions *options);
static const char *get_diff_type_name(int diff_type);
static const char *get_diff_type_color(int diff_type);

static void print_usage(const char *program)
{
    printf("FastDFS Configuration Compare Tool v1.0\n");
    printf("Compares two FastDFS configuration files\n\n");
    printf("Usage: %s [options] <config1> <config2>\n", program);
    printf("Options:\n");
    printf("  -f, --format <fmt>    Output format: text, json, html (default: text)\n");
    printf("  -o, --output <file>   Write output to file\n");
    printf("  -u, --unchanged       Show unchanged items\n");
    printf("  -c, --ignore-comments Ignore comment lines\n");
    printf("  -w, --ignore-ws       Ignore whitespace differences\n");
    printf("  -v, --verbose         Verbose output\n");
    printf("  -h, --help            Show this help\n\n");
    printf("Examples:\n");
    printf("  %s tracker1.conf tracker2.conf\n", program);
    printf("  %s -f json -o diff.json old.conf new.conf\n", program);
    printf("  %s -u --verbose storage1.conf storage2.conf\n", program);
}

static char *trim_string(char *str)
{
    char *end;
    
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    
    return str;
}

static int load_config_file(const char *filename, ConfigFile *config)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *key, *value, *eq;
    int line_number = 0;
    struct stat st;
    
    memset(config, 0, sizeof(ConfigFile));
    strncpy(config->filename, filename, MAX_PATH_LENGTH - 1);
    
    if (stat(filename, &st) == 0) {
        config->modified_time = st.st_mtime;
    }
    
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", 
                filename, strerror(errno));
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_number++;
        
        /* Remove newline */
        line[strcspn(line, "\r\n")] = '\0';
        
        /* Skip empty lines and comments */
        char *trimmed = trim_string(line);
        if (*trimmed == '\0' || *trimmed == '#') {
            continue;
        }
        
        /* Find equals sign */
        eq = strchr(trimmed, '=');
        if (eq == NULL) {
            continue;
        }
        
        /* Split key and value */
        *eq = '\0';
        key = trim_string(trimmed);
        value = trim_string(eq + 1);
        
        if (config->count < MAX_CONFIG_ITEMS) {
            strncpy(config->items[config->count].key, key, 63);
            strncpy(config->items[config->count].value, value, 255);
            config->items[config->count].line_number = line_number;
            config->count++;
        }
    }
    
    fclose(fp);
    return 0;
}

static const char *get_config_value(ConfigFile *config, const char *key)
{
    int i;
    for (i = 0; i < config->count; i++) {
        if (strcmp(config->items[i].key, key) == 0) {
            return config->items[i].value;
        }
    }
    return NULL;
}

static int find_config_item(ConfigFile *config, const char *key)
{
    int i;
    for (i = 0; i < config->count; i++) {
        if (strcmp(config->items[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static void compare_configs(ConfigFile *config1, ConfigFile *config2, 
                           DiffReport *report, CompareOptions *options)
{
    int i, j;
    int found;
    
    memset(report, 0, sizeof(DiffReport));
    
    /* Check items in config1 */
    for (i = 0; i < config1->count; i++) {
        j = find_config_item(config2, config1->items[i].key);
        
        if (j < 0) {
            /* Item removed in config2 */
            if (report->count < MAX_DIFF_ITEMS) {
                strncpy(report->items[report->count].key, 
                        config1->items[i].key, 63);
                strncpy(report->items[report->count].value1, 
                        config1->items[i].value, 255);
                report->items[report->count].value2[0] = '\0';
                report->items[report->count].diff_type = DIFF_REMOVED;
                report->items[report->count].line1 = config1->items[i].line_number;
                report->items[report->count].line2 = 0;
                report->count++;
                report->removed++;
            }
        } else {
            /* Check if value changed */
            int values_equal;
            if (options->ignore_whitespace) {
                char v1[256], v2[256];
                strcpy(v1, config1->items[i].value);
                strcpy(v2, config2->items[j].value);
                values_equal = (strcmp(trim_string(v1), trim_string(v2)) == 0);
            } else {
                values_equal = (strcmp(config1->items[i].value, 
                                       config2->items[j].value) == 0);
            }
            
            if (!values_equal) {
                /* Value modified */
                if (report->count < MAX_DIFF_ITEMS) {
                    strncpy(report->items[report->count].key, 
                            config1->items[i].key, 63);
                    strncpy(report->items[report->count].value1, 
                            config1->items[i].value, 255);
                    strncpy(report->items[report->count].value2, 
                            config2->items[j].value, 255);
                    report->items[report->count].diff_type = DIFF_MODIFIED;
                    report->items[report->count].line1 = config1->items[i].line_number;
                    report->items[report->count].line2 = config2->items[j].line_number;
                    report->count++;
                    report->modified++;
                }
            } else if (options->show_unchanged) {
                /* Unchanged */
                if (report->count < MAX_DIFF_ITEMS) {
                    strncpy(report->items[report->count].key, 
                            config1->items[i].key, 63);
                    strncpy(report->items[report->count].value1, 
                            config1->items[i].value, 255);
                    strncpy(report->items[report->count].value2, 
                            config2->items[j].value, 255);
                    report->items[report->count].diff_type = DIFF_UNCHANGED;
                    report->items[report->count].line1 = config1->items[i].line_number;
                    report->items[report->count].line2 = config2->items[j].line_number;
                    report->count++;
                    report->unchanged++;
                }
            }
        }
    }
    
    /* Check for items added in config2 */
    for (j = 0; j < config2->count; j++) {
        i = find_config_item(config1, config2->items[j].key);
        
        if (i < 0) {
            /* Item added in config2 */
            if (report->count < MAX_DIFF_ITEMS) {
                strncpy(report->items[report->count].key, 
                        config2->items[j].key, 63);
                report->items[report->count].value1[0] = '\0';
                strncpy(report->items[report->count].value2, 
                        config2->items[j].value, 255);
                report->items[report->count].diff_type = DIFF_ADDED;
                report->items[report->count].line1 = 0;
                report->items[report->count].line2 = config2->items[j].line_number;
                report->count++;
                report->added++;
            }
        }
    }
}

static const char *get_diff_type_name(int diff_type)
{
    switch (diff_type) {
        case DIFF_ADDED: return "ADDED";
        case DIFF_REMOVED: return "REMOVED";
        case DIFF_MODIFIED: return "MODIFIED";
        case DIFF_UNCHANGED: return "UNCHANGED";
        default: return "UNKNOWN";
    }
}

static const char *get_diff_type_color(int diff_type)
{
    switch (diff_type) {
        case DIFF_ADDED: return "\033[32m";    /* Green */
        case DIFF_REMOVED: return "\033[31m";  /* Red */
        case DIFF_MODIFIED: return "\033[33m"; /* Yellow */
        case DIFF_UNCHANGED: return "\033[0m"; /* Default */
        default: return "\033[0m";
    }
}

static void print_diff_report_text(DiffReport *report, ConfigFile *config1, 
                                   ConfigFile *config2, CompareOptions *options)
{
    int i;
    char time1[64], time2[64];
    
    printf("=== FastDFS Configuration Comparison ===\n\n");
    
    /* Format timestamps */
    strftime(time1, sizeof(time1), "%Y-%m-%d %H:%M:%S", 
             localtime(&config1->modified_time));
    strftime(time2, sizeof(time2), "%Y-%m-%d %H:%M:%S", 
             localtime(&config2->modified_time));
    
    printf("File 1: %s (%s)\n", config1->filename, time1);
    printf("File 2: %s (%s)\n\n", config2->filename, time2);
    
    printf("Summary:\n");
    printf("  Added:     %d\n", report->added);
    printf("  Removed:   %d\n", report->removed);
    printf("  Modified:  %d\n", report->modified);
    if (options->show_unchanged) {
        printf("  Unchanged: %d\n", report->unchanged);
    }
    printf("\n");
    
    if (report->count == 0) {
        printf("No differences found.\n");
        return;
    }
    
    printf("Details:\n");
    printf("%-30s %-10s %-30s %-30s\n", "Key", "Status", "File 1", "File 2");
    printf("%-30s %-10s %-30s %-30s\n", 
           "------------------------------", "----------",
           "------------------------------", "------------------------------");
    
    for (i = 0; i < report->count; i++) {
        DiffItem *item = &report->items[i];
        
        if (options->verbose) {
            printf("%s", get_diff_type_color(item->diff_type));
        }
        
        printf("%-30s %-10s %-30s %-30s\n",
               item->key,
               get_diff_type_name(item->diff_type),
               item->value1[0] ? item->value1 : "(not set)",
               item->value2[0] ? item->value2 : "(not set)");
        
        if (options->verbose) {
            printf("\033[0m");
        }
    }
}

static void print_diff_report_json(DiffReport *report, ConfigFile *config1, 
                                   ConfigFile *config2, CompareOptions *options)
{
    int i;
    FILE *out = stdout;
    
    if (options->output_file[0]) {
        out = fopen(options->output_file, "w");
        if (out == NULL) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", 
                    options->output_file);
            return;
        }
    }
    
    fprintf(out, "{\n");
    fprintf(out, "  \"file1\": \"%s\",\n", config1->filename);
    fprintf(out, "  \"file2\": \"%s\",\n", config2->filename);
    fprintf(out, "  \"summary\": {\n");
    fprintf(out, "    \"added\": %d,\n", report->added);
    fprintf(out, "    \"removed\": %d,\n", report->removed);
    fprintf(out, "    \"modified\": %d,\n", report->modified);
    fprintf(out, "    \"unchanged\": %d\n", report->unchanged);
    fprintf(out, "  },\n");
    fprintf(out, "  \"differences\": [\n");
    
    for (i = 0; i < report->count; i++) {
        DiffItem *item = &report->items[i];
        
        fprintf(out, "    {\n");
        fprintf(out, "      \"key\": \"%s\",\n", item->key);
        fprintf(out, "      \"status\": \"%s\",\n", get_diff_type_name(item->diff_type));
        fprintf(out, "      \"value1\": \"%s\",\n", item->value1);
        fprintf(out, "      \"value2\": \"%s\",\n", item->value2);
        fprintf(out, "      \"line1\": %d,\n", item->line1);
        fprintf(out, "      \"line2\": %d\n", item->line2);
        fprintf(out, "    }%s\n", (i < report->count - 1) ? "," : "");
    }
    
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    
    if (options->output_file[0] && out != stdout) {
        fclose(out);
        printf("Output written to %s\n", options->output_file);
    }
}

static void print_diff_report_html(DiffReport *report, ConfigFile *config1, 
                                   ConfigFile *config2, CompareOptions *options)
{
    int i;
    FILE *out = stdout;
    
    if (options->output_file[0]) {
        out = fopen(options->output_file, "w");
        if (out == NULL) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", 
                    options->output_file);
            return;
        }
    }
    
    fprintf(out, "<!DOCTYPE html>\n");
    fprintf(out, "<html>\n<head>\n");
    fprintf(out, "<title>FastDFS Configuration Comparison</title>\n");
    fprintf(out, "<style>\n");
    fprintf(out, "body { font-family: Arial, sans-serif; margin: 20px; }\n");
    fprintf(out, "h1 { color: #333; }\n");
    fprintf(out, "table { border-collapse: collapse; width: 100%%; }\n");
    fprintf(out, "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n");
    fprintf(out, "th { background-color: #4CAF50; color: white; }\n");
    fprintf(out, ".added { background-color: #c8e6c9; }\n");
    fprintf(out, ".removed { background-color: #ffcdd2; }\n");
    fprintf(out, ".modified { background-color: #fff9c4; }\n");
    fprintf(out, ".unchanged { background-color: #f5f5f5; }\n");
    fprintf(out, ".summary { margin: 20px 0; padding: 15px; background: #e3f2fd; }\n");
    fprintf(out, "</style>\n");
    fprintf(out, "</head>\n<body>\n");
    
    fprintf(out, "<h1>FastDFS Configuration Comparison</h1>\n");
    fprintf(out, "<p><strong>File 1:</strong> %s</p>\n", config1->filename);
    fprintf(out, "<p><strong>File 2:</strong> %s</p>\n", config2->filename);
    
    fprintf(out, "<div class=\"summary\">\n");
    fprintf(out, "<h3>Summary</h3>\n");
    fprintf(out, "<p>Added: %d | Removed: %d | Modified: %d | Unchanged: %d</p>\n",
            report->added, report->removed, report->modified, report->unchanged);
    fprintf(out, "</div>\n");
    
    fprintf(out, "<table>\n");
    fprintf(out, "<tr><th>Key</th><th>Status</th><th>File 1</th><th>File 2</th></tr>\n");
    
    for (i = 0; i < report->count; i++) {
        DiffItem *item = &report->items[i];
        const char *class_name;
        
        switch (item->diff_type) {
            case DIFF_ADDED: class_name = "added"; break;
            case DIFF_REMOVED: class_name = "removed"; break;
            case DIFF_MODIFIED: class_name = "modified"; break;
            default: class_name = "unchanged"; break;
        }
        
        fprintf(out, "<tr class=\"%s\">\n", class_name);
        fprintf(out, "  <td>%s</td>\n", item->key);
        fprintf(out, "  <td>%s</td>\n", get_diff_type_name(item->diff_type));
        fprintf(out, "  <td>%s</td>\n", item->value1[0] ? item->value1 : "(not set)");
        fprintf(out, "  <td>%s</td>\n", item->value2[0] ? item->value2 : "(not set)");
        fprintf(out, "</tr>\n");
    }
    
    fprintf(out, "</table>\n");
    fprintf(out, "<p><em>Generated by FastDFS Config Compare Tool</em></p>\n");
    fprintf(out, "</body>\n</html>\n");
    
    if (options->output_file[0] && out != stdout) {
        fclose(out);
        printf("Output written to %s\n", options->output_file);
    }
}

int main(int argc, char *argv[])
{
    ConfigFile config1, config2;
    DiffReport report;
    CompareOptions options;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"format", required_argument, 0, 'f'},
        {"output", required_argument, 0, 'o'},
        {"unchanged", no_argument, 0, 'u'},
        {"ignore-comments", no_argument, 0, 'c'},
        {"ignore-ws", no_argument, 0, 'w'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize options */
    memset(&options, 0, sizeof(options));
    options.output_format = OUTPUT_TEXT;
    
    /* Parse command line options */
    while ((opt = getopt_long(argc, argv, "f:o:ucwvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'f':
                if (strcmp(optarg, "json") == 0) {
                    options.output_format = OUTPUT_JSON;
                } else if (strcmp(optarg, "html") == 0) {
                    options.output_format = OUTPUT_HTML;
                } else {
                    options.output_format = OUTPUT_TEXT;
                }
                break;
            case 'o':
                strncpy(options.output_file, optarg, MAX_PATH_LENGTH - 1);
                break;
            case 'u':
                options.show_unchanged = 1;
                break;
            case 'c':
                options.ignore_comments = 1;
                break;
            case 'w':
                options.ignore_whitespace = 1;
                break;
            case 'v':
                options.verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Check for required arguments */
    if (optind + 2 > argc) {
        fprintf(stderr, "Error: Two configuration files required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Load configuration files */
    if (load_config_file(argv[optind], &config1) != 0) {
        return 1;
    }
    
    if (load_config_file(argv[optind + 1], &config2) != 0) {
        return 1;
    }
    
    /* Compare configurations */
    compare_configs(&config1, &config2, &report, &options);
    
    /* Print report */
    switch (options.output_format) {
        case OUTPUT_JSON:
            print_diff_report_json(&report, &config1, &config2, &options);
            break;
        case OUTPUT_HTML:
            print_diff_report_html(&report, &config1, &config2, &options);
            break;
        default:
            print_diff_report_text(&report, &config1, &config2, &options);
            break;
    }
    
    /* Return exit code based on differences */
    if (report.added > 0 || report.removed > 0 || report.modified > 0) {
        return 1;
    }
    
    return 0;
}
