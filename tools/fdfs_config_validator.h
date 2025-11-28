/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_config_validator.h
* Header file for FastDFS configuration validation utilities
*/

#ifndef FDFS_CONFIG_VALIDATOR_H
#define FDFS_CONFIG_VALIDATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum limits */
#define CV_MAX_LINE_LENGTH 1024
#define CV_MAX_PATH_LENGTH 256
#define CV_MAX_CONFIG_ITEMS 100
#define CV_MAX_VALIDATION_RULES 50
#define CV_MAX_MESSAGE_LENGTH 512

/* Validation result levels */
#define CV_LEVEL_OK 0
#define CV_LEVEL_INFO 1
#define CV_LEVEL_WARNING 2
#define CV_LEVEL_ERROR 3
#define CV_LEVEL_CRITICAL 4

/* Config types */
#define CV_CONFIG_TYPE_UNKNOWN 0
#define CV_CONFIG_TYPE_TRACKER 1
#define CV_CONFIG_TYPE_STORAGE 2
#define CV_CONFIG_TYPE_CLIENT 3

/* Validation rule types */
#define CV_RULE_REQUIRED 1
#define CV_RULE_RANGE 2
#define CV_RULE_PATH_EXISTS 3
#define CV_RULE_PATH_WRITABLE 4
#define CV_RULE_NETWORK 5
#define CV_RULE_CUSTOM 6

/**
 * Configuration item structure
 */
typedef struct cv_config_item {
    char key[64];
    char value[256];
    int line_number;
    int is_valid;
} CVConfigItem;

/**
 * Validation rule structure
 */
typedef struct cv_validation_rule {
    char key[64];
    int rule_type;
    int min_value;
    int max_value;
    int is_required;
    char description[256];
} CVValidationRule;

/**
 * Validation result structure
 */
typedef struct cv_validation_result {
    int level;
    char key[64];
    char message[CV_MAX_MESSAGE_LENGTH];
    char suggestion[CV_MAX_MESSAGE_LENGTH];
    int line_number;
} CVValidationResult;

/**
 * Configuration file structure
 */
typedef struct cv_config_file {
    CVConfigItem items[CV_MAX_CONFIG_ITEMS];
    int count;
    char filename[CV_MAX_PATH_LENGTH];
    int config_type;
    time_t load_time;
} CVConfigFile;

/**
 * Validation report structure
 */
typedef struct cv_validation_report {
    CVValidationResult results[CV_MAX_CONFIG_ITEMS * 2];
    int count;
    int info_count;
    int warning_count;
    int error_count;
    int critical_count;
    char config_filename[CV_MAX_PATH_LENGTH];
    time_t validation_time;
} CVValidationReport;

/**
 * Validation context structure
 */
typedef struct cv_validation_context {
    CVConfigFile *config;
    CVValidationReport *report;
    CVValidationRule rules[CV_MAX_VALIDATION_RULES];
    int rule_count;
    int verbose;
    int strict_mode;
} CVValidationContext;

/**
 * System information structure
 */
typedef struct cv_system_info {
    long total_memory_mb;
    long available_memory_mb;
    int cpu_count;
    long disk_space_mb;
    char hostname[256];
    char os_version[128];
} CVSystemInfo;

/* ============================================================
 * Configuration Loading Functions
 * ============================================================ */

/**
 * Initialize a config file structure
 * @param config Pointer to config file structure
 */
void cv_config_init(CVConfigFile *config);

/**
 * Load configuration from file
 * @param filename Path to configuration file
 * @param config Pointer to config file structure
 * @return 0 on success, -1 on error
 */
int cv_config_load(const char *filename, CVConfigFile *config);

/**
 * Free config file resources
 * @param config Pointer to config file structure
 */
void cv_config_free(CVConfigFile *config);

/**
 * Get configuration value by key
 * @param config Pointer to config file structure
 * @param key Configuration key
 * @return Value string or NULL if not found
 */
const char *cv_config_get_value(CVConfigFile *config, const char *key);

/**
 * Get configuration value as integer
 * @param config Pointer to config file structure
 * @param key Configuration key
 * @param default_val Default value if key not found
 * @return Integer value
 */
int cv_config_get_int(CVConfigFile *config, const char *key, int default_val);

/**
 * Get configuration value as long
 * @param config Pointer to config file structure
 * @param key Configuration key
 * @param default_val Default value if key not found
 * @return Long value
 */
long cv_config_get_long(CVConfigFile *config, const char *key, long default_val);

/**
 * Get configuration value as boolean
 * @param config Pointer to config file structure
 * @param key Configuration key
 * @param default_val Default value if key not found
 * @return Boolean value (0 or 1)
 */
int cv_config_get_bool(CVConfigFile *config, const char *key, int default_val);

/**
 * Check if configuration key exists
 * @param config Pointer to config file structure
 * @param key Configuration key
 * @return 1 if exists, 0 otherwise
 */
int cv_config_has_key(CVConfigFile *config, const char *key);

/**
 * Detect configuration type from content
 * @param config Pointer to config file structure
 * @return Config type constant
 */
int cv_config_detect_type(CVConfigFile *config);

/* ============================================================
 * Validation Report Functions
 * ============================================================ */

/**
 * Initialize validation report
 * @param report Pointer to validation report
 */
void cv_report_init(CVValidationReport *report);

/**
 * Add result to validation report
 * @param report Pointer to validation report
 * @param level Severity level
 * @param key Configuration key
 * @param line_number Line number in config file
 * @param message Error/warning message
 * @param suggestion Suggested fix
 */
void cv_report_add(CVValidationReport *report, int level, const char *key,
                   int line_number, const char *message, const char *suggestion);

/**
 * Add formatted result to validation report
 * @param report Pointer to validation report
 * @param level Severity level
 * @param key Configuration key
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void cv_report_add_formatted(CVValidationReport *report, int level,
                             const char *key, const char *format, ...);

/**
 * Print validation report to stdout
 * @param report Pointer to validation report
 * @param verbose Include detailed information
 */
void cv_report_print(CVValidationReport *report, int verbose);

/**
 * Export validation report to JSON
 * @param report Pointer to validation report
 * @param filename Output filename
 * @return 0 on success, -1 on error
 */
int cv_report_export_json(CVValidationReport *report, const char *filename);

/**
 * Export validation report to HTML
 * @param report Pointer to validation report
 * @param filename Output filename
 * @return 0 on success, -1 on error
 */
int cv_report_export_html(CVValidationReport *report, const char *filename);

/**
 * Get report summary string
 * @param report Pointer to validation report
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cv_report_get_summary(CVValidationReport *report, char *buffer, size_t buffer_size);

/**
 * Check if report has errors
 * @param report Pointer to validation report
 * @return 1 if has errors, 0 otherwise
 */
int cv_report_has_errors(CVValidationReport *report);

/**
 * Check if report has warnings
 * @param report Pointer to validation report
 * @return 1 if has warnings, 0 otherwise
 */
int cv_report_has_warnings(CVValidationReport *report);

/* ============================================================
 * Validation Context Functions
 * ============================================================ */

/**
 * Initialize validation context
 * @param ctx Pointer to validation context
 * @param config Pointer to config file
 * @param report Pointer to validation report
 */
void cv_context_init(CVValidationContext *ctx, CVConfigFile *config,
                     CVValidationReport *report);

/**
 * Add validation rule to context
 * @param ctx Pointer to validation context
 * @param key Configuration key
 * @param rule_type Rule type constant
 * @param min_value Minimum value (for range rules)
 * @param max_value Maximum value (for range rules)
 * @param is_required Whether key is required
 * @param description Rule description
 */
void cv_context_add_rule(CVValidationContext *ctx, const char *key,
                         int rule_type, int min_value, int max_value,
                         int is_required, const char *description);

/**
 * Set verbose mode
 * @param ctx Pointer to validation context
 * @param verbose Verbose flag
 */
void cv_context_set_verbose(CVValidationContext *ctx, int verbose);

/**
 * Set strict mode
 * @param ctx Pointer to validation context
 * @param strict Strict mode flag
 */
void cv_context_set_strict(CVValidationContext *ctx, int strict);

/* ============================================================
 * Validation Functions
 * ============================================================ */

/**
 * Validate tracker configuration
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_tracker(CVValidationContext *ctx);

/**
 * Validate storage configuration
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_storage(CVValidationContext *ctx);

/**
 * Validate client configuration
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_client(CVValidationContext *ctx);

/**
 * Validate common settings
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_common(CVValidationContext *ctx);

/**
 * Validate network settings
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_network(CVValidationContext *ctx);

/**
 * Validate path settings
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_paths(CVValidationContext *ctx);

/**
 * Validate performance settings
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_performance(CVValidationContext *ctx);

/**
 * Validate security settings
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_security(CVValidationContext *ctx);

/**
 * Run all validations based on config type
 * @param ctx Pointer to validation context
 * @return Number of errors found
 */
int cv_validate_all(CVValidationContext *ctx);

/* ============================================================
 * System Information Functions
 * ============================================================ */

/**
 * Get system information
 * @param info Pointer to system info structure
 * @return 0 on success, -1 on error
 */
int cv_get_system_info(CVSystemInfo *info);

/**
 * Get available memory in MB
 * @return Available memory in MB
 */
long cv_get_available_memory_mb(void);

/**
 * Get CPU count
 * @return Number of CPUs
 */
int cv_get_cpu_count(void);

/**
 * Get available disk space in MB
 * @param path Path to check
 * @return Available disk space in MB
 */
long cv_get_disk_space_mb(const char *path);

/**
 * Check if path exists
 * @param path Path to check
 * @return 1 if exists, 0 otherwise
 */
int cv_path_exists(const char *path);

/**
 * Check if path is writable
 * @param path Path to check
 * @return 1 if writable, 0 otherwise
 */
int cv_path_writable(const char *path);

/**
 * Check if path is a directory
 * @param path Path to check
 * @return 1 if directory, 0 otherwise
 */
int cv_path_is_directory(const char *path);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Trim whitespace from string
 * @param str String to trim
 * @return Trimmed string
 */
char *cv_trim_string(char *str);

/**
 * Parse size string (e.g., "1G", "512M", "1024K")
 * @param str Size string
 * @return Size in bytes
 */
long cv_parse_size(const char *str);

/**
 * Parse time string (e.g., "1d", "12h", "30m", "60s")
 * @param str Time string
 * @return Time in seconds
 */
int cv_parse_time(const char *str);

/**
 * Format size for display
 * @param bytes Size in bytes
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cv_format_size(long bytes, char *buffer, size_t buffer_size);

/**
 * Format time for display
 * @param seconds Time in seconds
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cv_format_time(int seconds, char *buffer, size_t buffer_size);

/**
 * Get level name string
 * @param level Severity level
 * @return Level name string
 */
const char *cv_get_level_name(int level);

/**
 * Get level color code (ANSI)
 * @param level Severity level
 * @return ANSI color code string
 */
const char *cv_get_level_color(int level);

/**
 * Compare two configuration files
 * @param config1 First config file
 * @param config2 Second config file
 * @param report Output report
 * @return Number of differences
 */
int cv_compare_configs(CVConfigFile *config1, CVConfigFile *config2,
                       CVValidationReport *report);

/**
 * Generate recommended configuration
 * @param info System information
 * @param config_type Config type
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return 0 on success, -1 on error
 */
int cv_generate_recommended_config(CVSystemInfo *info, int config_type,
                                   char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* FDFS_CONFIG_VALIDATOR_H */
