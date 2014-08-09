#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "common_define.h"
#include "test_types.h"

typedef struct {
	int bytes;
	char *filename;
} TestFileInfo;

TestFileInfo files[FILE_TYPE_COUNT] = {
	{5 * 1024, "5K"},
	{50 * 1024, "50K"}, 
	{200 * 1024, "200K"},
	{1 * 1024 * 1024, "1M"},
	{10 * 1024 * 1024, "10M"},
	{100 * 1024 * 1024, "100M"}
};

int main()
{
#define BUFF_SIZE  (1 * 1024)
	int i;
	int k;
	int loop;
	FILE *fp;
	unsigned char buff[BUFF_SIZE];
	unsigned char *p;
	unsigned char *pEnd;

	srand(SRAND_SEED);
	pEnd = buff + BUFF_SIZE;
	for (i=0; i<FILE_TYPE_COUNT; i++)
	{
		fp = fopen(files[i].filename, "wb");
		if (fp == NULL)
		{
			printf("open file %s fail, errno: %d, error info: %s\n", 
				files[i].filename, errno, STRERROR(errno));
			return 1;
		}

		loop = files[i].bytes / BUFF_SIZE;
		for (k=0; k<loop-1; k++)
		{
			for (p=buff; p<pEnd; p++)
			{
				*p = (int)(255 * ((double)rand() / RAND_MAX));
			}

			if (fwrite(buff, BUFF_SIZE, 1, fp) != 1)
			{
				printf("write file %s fail, errno: %d, error info: %s\n", 
					files[i].filename, errno, STRERROR(errno));
		        fclose(fp);
				return 1;
			}
		}

		memset(buff, 0xFF, BUFF_SIZE);
		if (fwrite(buff, BUFF_SIZE, 1, fp) != 1)
		{
			printf("write file %s fail, errno: %d, error info: %s\n", 
				files[i].filename, errno, STRERROR(errno));
		    fclose(fp);
			return 1;
		}

		fclose(fp);
	}

	printf("done.\n");

	return 0;
}

