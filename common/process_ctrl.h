
#ifndef PROCESS_CTRL_H
#define PROCESS_CTRL_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef __cplusplus
extern "C" {
#endif

int get_base_path_from_conf_file(const char *filename, char *base_path,
	const int path_size);

int get_pid_from_file(const char *pidFilename, pid_t *pid);

int write_to_pid_file(const char *pidFilename);

int delete_pid_file(const char *pidFilename);

int process_stop(const char *pidFilename);

int process_restart(const char *pidFilename);

int process_exist(const char *pidFilename);

int process_action(const char *pidFilename, const char *action, bool *stop);

#ifdef __cplusplus
}
#endif

#endif

