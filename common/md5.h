#ifndef MCL_MD5_H
#define MCL_MD5_H
#include <stdio.h>

typedef unsigned char *POINTER;
typedef unsigned short int UINT2;
typedef unsigned int UINT4;

typedef struct {
  UINT4 state[4];		/* state (ABCD) */
  UINT4 count[2];		/* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];	/* input buffer */
} MD5_CTX;

#ifdef __cplusplus
extern "C" {
#endif

/** md5 for string
 *  parameters:
 *           string: the string to md5
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int my_md5_string(char *string, unsigned char digest[16]);

/** md5 for file
 *  parameters:
 *           filename: the filename whose content to md5
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int my_md5_file(char *filename, unsigned char digest[16]);

/** md5 for buffer
 *  parameters:
 *           buffer: the buffer to md5
 *           len: the buffer length
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int my_md5_buffer(char *buffer, unsigned int len, unsigned char digest[16]);

void my_md5_init (MD5_CTX *);

void my_md5_update (MD5_CTX *, unsigned char *, unsigned int);

void my_md5_final (unsigned char [16], MD5_CTX *);

#ifdef __cplusplus
}
#endif

#endif

