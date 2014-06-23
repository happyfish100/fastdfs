/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#ifndef SHARED_FUNC_H
#define SHARED_FUNC_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "common_define.h"
#include "ini_file_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/** lowercase the string
 *  parameters:
 *  	src: input string, will be changed
 *  return: lowercased string
*/
char *toLowercase(char *src);

/** uppercase the string
 *  parameters:
 *  	src: input string, will be changed
 *  return: uppercased string
*/
char *toUppercase(char *src);


/** date format to string
 *  parameters:
 *  	nTime: unix timestamp
 *  	szDateFormat: date format, more detail man strftime
 *  	buff: store the formated result, can be NULL
 *  	buff_size: buffer size, max bytes can contain
 *  return: formated date string
*/
char *formatDatetime(const time_t nTime, \
	const char *szDateFormat, \
	char *buff, const int buff_size);

/** get character count, only support GB charset
 *  parameters:
 *  	s: the string
 *  return: character count
*/
int getCharLen(const char *s);

/** replace \r and \n to space
 *  parameters:
 *  	s: the string
 *  return: replaced string
*/
char *replaceCRLF2Space(char *s);

/** get the filename absolute path
 *  parameters:
 *  	fileame: the filename
 *  	szAbsPath: store the absolute path
 *  	pathSize: max bytes to contain
 *  return: absolute path, NULL for fail
*/
char *getAbsolutePath(const char *fileame, char *szAbsPath, \
				const int pathSize);

/** get the executable file absolute filename
 *  parameters:
 *  	exeFilename: the executable filename
 *  	szAbsFilename: store the absolute filename
 *  	maxSize: max bytes to contain
 *  return: absolute filename, NULL for fail
*/
char *getExeAbsoluteFilename(const char *exeFilename, char *szAbsFilename, \
		const int maxSize);

#ifndef WIN32

/** get running process count by program name such as fdfs_trackerd
 *  parameters:
 *  	progName: the program name
 * 	bAllOwners: false for only get my proccess count
 *  return: proccess count, >= 0 success, < 0 fail
*/
int getProccessCount(const char *progName, const bool bAllOwners);

/** get running process ids by program name such as fdfs_trackerd
 *  parameters:
 *  	progName: the program name
 * 	bAllOwners: false for only get my proccess count
 * 	pids: store the pids
 * 	arrSize: max pids
 *  return: proccess count, >= 0 success, < 0 fail
*/
int getUserProcIds(const char *progName, const bool bAllOwners, \
			int pids[], const int arrSize);

/** execute program, get it's output
 *  parameters:
 *  	command: the program
 * 	output: store ouput result
 * 	buff_size: output max size (bytes)
 *  return: error no, 0 success, != 0 fail
*/
int getExecResult(const char *command, char *output, const int buff_size);

#endif

/** daemon init
 *  parameters:
 *  	bCloseFiles: if close the stdin, stdout and stderr
 *  return: none
*/
void daemon_init(bool bCloseFiles);

/** convert buffer content to hex string such as 0B82A1
 *  parameters:
 *  	s: the buffer
 *  	len: the buffer length
 *  	szHexBuff: store the hex string (must have enough space)
 *  return: hex string (szHexBuff)
*/
char *bin2hex(const char *s, const int len, char *szHexBuff);

/** parse hex string to binary content
 *  parameters:
 *  	s: the hex string such as 8B04CD
 *  	szBinBuff: store the converted binary content(must have enough space)
 *  	nDestLen: store the converted content length
 *  return: converted binary content (szBinBuff)
*/
char *hex2bin(const char *s, char *szBinBuff, int *nDestLen);

/** print binary buffer as hex string
 *  parameters:
 *  	s: the buffer
 *  	len: the buffer length
 *  return: none
*/
void printBuffHex(const char *s, const int len);

/** 32 bits int convert to buffer (big-endian)
 *  parameters:
 *  	n: 32 bits int value
 *  	buff: the buffer, at least 4 bytes space, no tail \0
 *  return: none
*/
void int2buff(const int n, char *buff);

/** buffer convert to 32 bits int
 *  parameters:
 *  	buff: big-endian 4 bytes buffer
 *  return: 32 bits int value
*/
int buff2int(const char *buff);

/** long (64 bits) convert to buffer (big-endian)
 *  parameters:
 *  	n: 64 bits int value
 *  	buff: the buffer, at least 8 bytes space, no tail \0
 *  return: none
*/
void long2buff(int64_t n, char *buff);

/** buffer convert to 64 bits int
 *  parameters:
 *  	buff: big-endian 8 bytes buffer
 *  return: 64 bits int value
*/
int64_t buff2long(const char *buff);

/** trim leading spaces ( \t\r\n)
 *  parameters:
 *  	pStr: the string to trim
 *  return: trimed string porinter as pStr
*/
char *trim_left(char *pStr);

/** trim tail spaces ( \t\r\n)
 *  parameters:
 *  	pStr: the string to trim
 *  return: trimed string porinter as pStr
*/
char *trim_right(char *pStr);

/** trim leading and tail spaces ( \t\r\n)
 *  parameters:
 *  	pStr: the string to trim
 *  return: trimed string porinter as pStr
*/
char *trim(char *pStr);

/** copy string to BufferInfo
 *  parameters:
 *  	pBuff: the dest buffer
 *  	str: source string
 *  return: error no, 0 success, != 0 fail
*/
int buffer_strcpy(BufferInfo *pBuff, const char *str);

/** copy binary buffer to BufferInfo
 *  parameters:
 *  	pBuff: the dest buffer
 *  	buff: source buffer
 *  	len: source buffer length
 *  return: error no, 0 success, != 0 fail
*/
int buffer_memcpy(BufferInfo *pBuff, const char *buff, const int len);

/** url encode
 *  parameters:
 *  	src: the source string to encode
 *  	src_len: source string length
 *  	dest: store dest string
 *  	dest_len: store the dest string length
 *  return: error no, 0 success, != 0 fail
*/
char *urlencode(const char *src, const int src_len, char *dest, int *dest_len);

/** url decode
 *  parameters:
 *  	src: the source string to decode
 *  	src_len: source string length
 *  	dest: store dest string
 *  	dest_len: store the dest string length
 *  return: error no, 0 success, != 0 fail
*/
char *urldecode(const char *src, const int src_len, char *dest, int *dest_len);

/** get char occurs count
 *  parameters:
 *  	src: the source string
 *  	seperator: find this char occurs times
 *  return: char occurs count
*/
int getOccurCount(const char *src, const char seperator);

/** split string
 *  parameters:
 *  	src: the source string, will be modified by this function
 *  	seperator: seperator char
 *  	nMaxCols: max columns (max split count)
 *  	nColCount: store the columns (array elements) count
 *  return: string array, should call freeSplit to free, return NULL when fail
*/
char **split(char *src, const char seperator, const int nMaxCols, \
		int *nColCount);

/** free split results
 *  parameters:
 *  	p: return by function split
 *  return: none
*/
void freeSplit(char **p);


/** split string
 *  parameters:
 *  	src: the source string, will be modified by this function
 *  	seperator: seperator char
 *  	pCols: store split strings
 *  	nMaxCols: max columns (max split count)
 *  return: string array / column count
*/
int splitEx(char *src, const char seperator, char **pCols, const int nMaxCols);

/** split string
 *  parameters:
 *  	src: the source string, will be modified by this function
 *  	seperator: seperator char
 *  	pCols: store split strings
 *  	nMaxCols: max columns (max split count)
 *  return: string array / column count
*/
int my_strtok(char *src, const char *delim, char **pCols, const int nMaxCols);

/** check file exist
 *  parameters:
 *  	filename: the filename
 *  return: true if file exists, otherwise false
*/
bool fileExists(const char *filename);

/** check if a directory
 *  parameters:
 *  	filename: the filename
 *  return: true for directory
*/
bool isDir(const char *filename);

/** check if a regular file
 *  parameters:
 *  	filename: the filename
 *  return: true for regular file
*/
bool isFile(const char *filename);

/** check if filename securty, /../ ocur in filename not allowed
 *  parameters:
 *  	filename: the filename
 *  	len: filename length
 *  return: true for regular file
*/
bool is_filename_secure(const char *filename, const int len);

/** load log_level from config context
 *  parameters:
 *  	pIniContext: the config context
 *  return: none
*/
void load_log_level(IniContext *pIniContext);

/** load log_level from config file
 *  parameters:
 *  	conf_filename: the config filename
 *  return: none
*/
int load_log_level_ex(const char *conf_filename);

/** set global log level
 *  parameters:
 *  	pLogLevel: log level string value
 *  return: none
*/
void set_log_level(char *pLogLevel);

/** load allow hosts from config context
 *  parameters:
 *  	pIniContext: the config context
 *  	allow_ip_addrs: store allow ip addresses
 *  	allow_ip_count: store allow ip address count
 *  return: error no , 0 success, != 0 fail
*/
int load_allow_hosts(IniContext *pIniContext, \
		in_addr_t **allow_ip_addrs, int *allow_ip_count);

/** get time item from config context
 *  parameters:
 *  	pIniContext: the config context
 *  	item_name: item name in config file, time format: hour:minute, such as 15:25
 *  	pTimeInfo: store time info
 *  	default_hour: default hour value
 *  	default_minute: default minute value
 *  return: error no , 0 success, != 0 fail
*/
int get_time_item_from_conf(IniContext *pIniContext, \
		const char *item_name, TimeInfo *pTimeInfo, \
		const byte default_hour, const byte default_minute);

/** trim path tail char /
 *  parameters:
 *  	filePath: the file path to chop
 *  return: none
*/
void chopPath(char *filePath);

/** get file content
 *  parameters:
 *  	filename: the filename
 *  	buff: return the buff, must be freed
 *  	file_size: store the file size
 *  return: error no , 0 success, != 0 fail
*/
int getFileContent(const char *filename, char **buff, int64_t *file_size);

/** get file content
 *  parameters:
 *  	filename: the filename
 *  	buff: the buff to store file content
 *      offset: the start offset
 *  	size: specify the size to fetch and return the fetched size
 *  return: error no , 0 success, != 0 fail
*/
int getFileContentEx(const char *filename, char *buff, \
		int64_t offset, int64_t *size);

/** write to file
 *  parameters:
 *  	filename: the filename to write
 *  	buff: the buffer to write
 *  	file_size: the file size
 *  return: error no , 0 success, != 0 fail
*/
int writeToFile(const char *filename, const char *buff, const int file_size);

/** safe write to file, first write to tmp file, then rename to true filename
 *  parameters:
 *  	filename: the filename to write
 *  	buff: the buffer to write
 *  	file_size: the file size
 *  return: error no , 0 success, != 0 fail
*/
int safeWriteToFile(const char *filename, const char *buff, \
		const int file_size);

/** get a line from file
 *  parameters:
 *  	fd: the fd to read
 *  	buff: the buffer to store the line
 *  	size: the buffer max size
 *  	once_bytes: the bytes per read
 *  return: error no , 0 success, != 0 fail
*/
int fd_gets(int fd, char *buff, const int size, int once_bytes);

/** set unix rlimit
 *  parameters:
 *  	resource: resource id, please see sys/resource.h
 *  	value: the value to set
 *  return: error no , 0 success, != 0 fail
*/
int set_rlimit(int resource, const rlim_t value);

/** set non block mode
 *  parameters:
 *  	fd: the fd to set
 *  	adding_flags: the flags to add
 *  return: error no , 0 success, != 0 fail
*/
int fd_add_flags(int fd, int adding_flags);

/** set non block mode
 *  parameters:
 *  	fd: the fd to set
 *  return: error no , 0 success, != 0 fail
*/
#define set_nonblock(fd) fd_add_flags(fd, O_NONBLOCK)

/** set run by group and user
 *  parameters:
 *  	group_name: the group name, can be NULL or empty
 *  	username: the username, can be NULL or empty
 *  return: error no , 0 success, != 0 fail
*/
int set_run_by(const char *group_name, const char *username);

/** compare ip address, type is (in_addr_t *)
 *  parameters:
 *  	p1: the first ip address
 *  	p2: the second ip address
 *  return: > 0 when p1 > p2, 0 when p1 == p2, < 0 when p1 < p2
*/
int cmp_by_ip_addr_t(const void *p1, const void *p2);

/** parse bytes
 *  parameters:
 *  	pStr: the string to parse
 *  	default_unit_bytes: default unit if not specified the unit like MB etc.
 *  	bytes: store the parsed bytes
 *  return: error no , 0 success, != 0 fail
*/
int parse_bytes(char *pStr, const int default_unit_bytes, int64_t *bytes);

/** set rand seed
 *  return: error no , 0 success, != 0 fail
*/
int set_rand_seed();

/** set timer wrapper
 *  parameters:
 *  	first_remain_seconds: remain time for first time, in seconds
 *  	interval: the interval
 *  	sighandler: handler function
 *  return: error no , 0 success, != 0 fail
*/
int set_timer(const int first_remain_seconds, const int interval, \
		void (*sighandler)(int));

/** set file access and modified times
 *  parameters:
 *  	filename: the file to modify times
 *  	new_time: the time to set
 *  return: error no , 0 success, != 0 fail
*/
int set_file_utimes(const char *filename, const time_t new_time);

/** ignore singal pipe (SIGPIPE)
 *  return: error no , 0 success, != 0 fail
*/
int ignore_signal_pipe();

#ifdef __cplusplus
}
#endif

#endif

