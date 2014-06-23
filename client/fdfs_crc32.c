/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "hash.h"

int main(int argc, char *argv[])
{
	int64_t file_size;
	int64_t remain_bytes;
	char *filename;
	int fd;
	int read_bytes;
	int result;
	int crc32;
	char buff[512 * 1024];

	if (argc < 2)
	{
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	filename = argv[1];
	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		printf("file: "__FILE__", line: %d, " \
			"open file %s fail, " \
			"errno: %d, error info: %s\n", \
			__LINE__, filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EACCES;
	}

	if ((file_size=lseek(fd, 0, SEEK_END)) < 0)
	{
		printf("file: "__FILE__", line: %d, " \
		       "call lseek fail, " \
			"errno: %d, error info: %s\n", \
			__LINE__, errno, STRERROR(errno));
		close(fd);
		return errno;
	}

	if (lseek(fd, 0, SEEK_SET) < 0)
	{
		printf("file: "__FILE__", line: %d, " \
		       "call lseek fail, " \
			"errno: %d, error info: %s\n", \
			__LINE__, errno, STRERROR(errno));
		close(fd);
		return errno;
	}

	crc32 = CRC32_XINIT;
	result = 0;
	remain_bytes = file_size;
	while (remain_bytes > 0)
	{
		if (remain_bytes > sizeof(buff))
		{
			read_bytes = sizeof(buff);
		}
		else
		{
			read_bytes = remain_bytes;
		}

		if (read(fd, buff, read_bytes) != read_bytes)
		{
			printf("file: "__FILE__", line: %d, " \
				"call lseek fail, " \
				"errno: %d, error info: %s\n", \
				__LINE__, errno, STRERROR(errno));
			result = errno != 0 ? errno : EIO;
			break;
		}

		crc32 = CRC32_ex(buff, read_bytes, crc32);
		remain_bytes -= read_bytes;
	}

	close(fd);

	if (result == 0)
	{
		crc32 = CRC32_FINAL(crc32);
		printf("%u\n", crc32);
	}

	return result;
}

