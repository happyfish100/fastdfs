//common_func.h

#ifndef _COMMON_FUNC_H
#define _COMMON_FUNC_H

#ifdef __cplusplus
extern "C" {
#endif

int getFileContent(const char *filename, char **buff, int64_t *file_size);

#ifdef __cplusplus
}
#endif

#endif
