/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//ini_file_reader.c

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "shared_func.h"
#include "logger.h"
#include "http_func.h"
#include "ini_file_reader.h"

#define _LINE_BUFFER_SIZE	512
#define _ALLOC_ITEMS_ONCE	8

static int iniDoLoadFromFile(const char *szFilename, \
		IniContext *pContext);
static int iniDoLoadItemsFromBuffer(char *content, \
		IniContext *pContext);

static int iniCompareByItemName(const void *p1, const void *p2)
{
	return strcmp(((IniItem *)p1)->name, ((IniItem *)p2)->name);
}

static int iniInitContext(IniContext *pContext)
{
	int result;

	memset(pContext, 0, sizeof(IniContext));
	pContext->current_section = &pContext->global;
	if ((result=hash_init(&pContext->sections, PJWHash, 10, 0.75)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"hash_init fail, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return result;
}

static int iniSortHashData(const int index, const HashData *data, void *args)
{
	IniSection *pSection;

	pSection = (IniSection *)data->value;
	if (pSection->count > 1)
	{
		qsort(pSection->items, pSection->count, \
			sizeof(IniItem), iniCompareByItemName);
	}

	return 0;
}

static void iniSortItems(IniContext *pContext)
{
	if (pContext->global.count > 1)
	{
		qsort(pContext->global.items, pContext->global.count, \
			sizeof(IniItem), iniCompareByItemName);
	}

	hash_walk(&pContext->sections, iniSortHashData, NULL);
}

int iniLoadFromFile(const char *szFilename, IniContext *pContext)
{
	int result;
	int len;
	char *pLast;
	char full_filename[MAX_PATH_SIZE];

	if ((result=iniInitContext(pContext)) != 0)
	{
		return result;
	}

	if (strncasecmp(szFilename, "http://", 7) == 0)
	{
		*pContext->config_path = '\0';
		snprintf(full_filename, sizeof(full_filename),"%s",szFilename);
	}
	else
	{
		if (*szFilename == '/')
		{
			pLast = strrchr(szFilename, '/');
			len = pLast - szFilename;
			if (len >= sizeof(pContext->config_path))
			{
				logError("file: "__FILE__", line: %d, "\
					"the path of the config file: %s is " \
					"too long!", __LINE__, szFilename);
				return ENOSPC;
			}

			memcpy(pContext->config_path, szFilename, len);
			*(pContext->config_path + len) = '\0';
			snprintf(full_filename, sizeof(full_filename), \
				"%s", szFilename);
		}
		else
		{
			memset(pContext->config_path, 0, \
				sizeof(pContext->config_path));
			if (getcwd(pContext->config_path, sizeof( \
				pContext->config_path)) == NULL)
			{
				logError("file: "__FILE__", line: %d, " \
					"getcwd fail, errno: %d, " \
					"error info: %s", \
					__LINE__, errno, STRERROR(errno));
				return errno != 0 ? errno : EPERM;
			}

			len = strlen(pContext->config_path);
			if (len > 0 && pContext->config_path[len - 1] == '/')
			{
				len--;
				*(pContext->config_path + len) = '\0';
			}

			snprintf(full_filename, sizeof(full_filename), \
				"%s/%s", pContext->config_path, szFilename);

			pLast = strrchr(szFilename, '/');
			if (pLast != NULL)
			{
				int tail_len;

				tail_len = pLast - szFilename;
				if (len + tail_len >= sizeof( \
						pContext->config_path))
				{
					logError("file: "__FILE__", line: %d, "\
						"the path of the config " \
						"file: %s is too long!", \
						__LINE__, szFilename);
					return ENOSPC;
				}

				memcpy(pContext->config_path + len, \
					szFilename, tail_len);
				len += tail_len;
				*(pContext->config_path + len) = '\0';
			}
		}
	}

	result = iniDoLoadFromFile(full_filename, pContext);
	if (result == 0)
	{
		iniSortItems(pContext);
	}
	else
	{
		iniFreeContext(pContext);
	}

	return result;
}

static int iniDoLoadFromFile(const char *szFilename, \
		IniContext *pContext)
{
	char *content;
	int result;
	int http_status;
	int content_len;
	int64_t file_size;
	char error_info[512];

	if (strncasecmp(szFilename, "http://", 7) == 0)
	{
		if ((result=get_url_content(szFilename, 10, 60, &http_status, \
				&content, &content_len, error_info)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"get_url_content fail, " \
				"url: %s, error info: %s", \
				__LINE__, szFilename, error_info);
			return result;
		}

		if (http_status != 200)
		{
			free(content);
			logError("file: "__FILE__", line: %d, " \
				"HTTP status code: %d != 200, url: %s", \
				__LINE__, http_status, szFilename);
			return EINVAL;
		}
	}
	else
	{
		if ((result=getFileContent(szFilename, &content, \
				&file_size)) != 0)
		{
			return result;
		}
	}

	result = iniDoLoadItemsFromBuffer(content, pContext);
	free(content);

	return result;
}

int iniLoadFromBuffer(char *content, IniContext *pContext)
{
	int result;

	if ((result=iniInitContext(pContext)) != 0)
	{
		return result;
	}

	result = iniDoLoadItemsFromBuffer(content, pContext);
	if (result == 0)
	{
		iniSortItems(pContext);
	}
	else
	{
		iniFreeContext(pContext);
	}

	return result;
}

static int iniDoLoadItemsFromBuffer(char *content, IniContext *pContext)
{
	IniSection *pSection;
	IniItem *pItem;
	char *pLine;
	char *pLastEnd;
	char *pEqualChar;
	char *pIncludeFilename;
	char full_filename[MAX_PATH_SIZE];
	int nLineLen;
	int nNameLen;
	int nValueLen;
	int result;

	result = 0;
	pLastEnd = content - 1;
	pSection = pContext->current_section;
	if (pSection->count > 0)
	{
		pItem = pSection->items + pSection->count;
	}
	else
	{
		pItem = pSection->items;
	}

	while (pLastEnd != NULL)
	{
		pLine = pLastEnd + 1;
		pLastEnd = strchr(pLine, '\n');
		if (pLastEnd != NULL)
		{
			*pLastEnd = '\0';
		}

		if (*pLine == '#' && \
			strncasecmp(pLine+1, "include", 7) == 0 && \
			(*(pLine+8) == ' ' || *(pLine+8) == '\t'))
		{
			pIncludeFilename = strdup(pLine + 9);
			if (pIncludeFilename == NULL)
			{
				logError("file: "__FILE__", line: %d, " \
					"strdup %d bytes fail", __LINE__, \
					(int)strlen(pLine + 9) + 1);
				result = errno != 0 ? errno : ENOMEM;
				break;
			}

			trim(pIncludeFilename);
			if (strncasecmp(pIncludeFilename, "http://", 7) == 0)
			{
				snprintf(full_filename, sizeof(full_filename),\
					"%s", pIncludeFilename);
			}
			else
			{
				if (*pIncludeFilename == '/')
				{
				snprintf(full_filename, sizeof(full_filename), \
					"%s", pIncludeFilename);
				}
				else
				{
				snprintf(full_filename, sizeof(full_filename), \
					"%s/%s", pContext->config_path, \
					 pIncludeFilename);
				}

				if (!fileExists(full_filename))
				{
				logError("file: "__FILE__", line: %d, " \
					"include file \"%s\" not exists, " \
					"line: \"%s\"", __LINE__, \
					pIncludeFilename, pLine);
				free(pIncludeFilename);
				result = ENOENT;
				break;
				}
			}

			result = iniDoLoadFromFile(full_filename, pContext);
			if (result != 0)
			{
				free(pIncludeFilename);
				break;
			}

			pSection = pContext->current_section;
			if (pSection->count > 0)
			{
				pItem = pSection->items + pSection->count;  //must re-asign
			}
			else
			{
				pItem = pSection->items;
			}

			free(pIncludeFilename);
			continue;
		}

		trim(pLine);
		if (*pLine == '#' || *pLine == '\0')
		{
			continue;
		}

		nLineLen = strlen(pLine);
		if (*pLine == '[' && *(pLine + (nLineLen - 1)) == ']') //section
		{
			char *section_name;
			int section_len;

			*(pLine + (nLineLen - 1)) = '\0';
			section_name = pLine + 1; //skip [

			trim(section_name);
			if (*section_name == '\0') //global section
			{
				pContext->current_section = &pContext->global;
				pSection = pContext->current_section;
				if (pSection->count > 0)
				{
					pItem = pSection->items + pSection->count;
				}
				else
				{
					pItem = pSection->items;
				}
				continue;
			}

			section_len = strlen(section_name);
			pSection = (IniSection *)hash_find(&pContext->sections,\
					section_name, section_len);
			if (pSection == NULL)
			{
				pSection = (IniSection *)malloc(sizeof(IniSection));
				if (pSection == NULL)
				{
					result = errno != 0 ? errno : ENOMEM;
					logError("file: "__FILE__", line: %d, "\
						"malloc %d bytes fail, " \
						"errno: %d, error info: %s", \
						__LINE__, \
						(int)sizeof(IniSection), \
						result, STRERROR(result));
					
					break;
				}

				memset(pSection, 0, sizeof(IniSection));
				result = hash_insert(&pContext->sections, \
					  section_name, section_len, pSection);
				if (result < 0)
				{
					result *= -1;
					logError("file: "__FILE__", line: %d, "\
						"insert into hash table fail, "\
						"errno: %d, error info: %s", \
						__LINE__, result, \
						STRERROR(result));
					break;
				}
				else
				{
					result = 0;
				}
			}

			pContext->current_section = pSection;
			if (pSection->count > 0)
			{
				pItem = pSection->items + pSection->count;
			}
			else
			{
				pItem = pSection->items;
			}
			continue;
		}
		
		pEqualChar = strchr(pLine, '=');
		if (pEqualChar == NULL)
		{
			continue;
		}
		
		nNameLen = pEqualChar - pLine;
		nValueLen = strlen(pLine) - (nNameLen + 1);
		if (nNameLen > FAST_INI_ITEM_NAME_LEN)
		{
			nNameLen = FAST_INI_ITEM_NAME_LEN;
		}
		
		if (nValueLen > FAST_INI_ITEM_VALUE_LEN)
		{
			nValueLen = FAST_INI_ITEM_VALUE_LEN;
		}
	
		if (pSection->count >= pSection->alloc_count)
		{
			pSection->alloc_count += _ALLOC_ITEMS_ONCE;
			pSection->items=(IniItem *)realloc(pSection->items, 
				sizeof(IniItem) * pSection->alloc_count);
			if (pSection->items == NULL)
			{
				logError("file: "__FILE__", line: %d, " \
					"realloc %d bytes fail", __LINE__, \
					(int)sizeof(IniItem) * \
					pSection->alloc_count);
				result = errno != 0 ? errno : ENOMEM;
				break;
			}

			pItem = pSection->items + pSection->count;
			memset(pItem, 0, sizeof(IniItem) * \
				(pSection->alloc_count - pSection->count));
		}

		memcpy(pItem->name, pLine, nNameLen);
		memcpy(pItem->value, pEqualChar + 1, nValueLen);
		
		trim(pItem->name);
		trim(pItem->value);
		
		pSection->count++;
		pItem++;
	}

	return result;
}

static int iniFreeHashData(const int index, const HashData *data, void *args)
{
	IniSection *pSection;

	pSection = (IniSection *)data->value;
	if (pSection == NULL)
	{
		return 0;
	}

	if (pSection->items != NULL)
	{
		free(pSection->items);
		memset(pSection, 0, sizeof(IniSection));
	}

	free(pSection);
	((HashData *)data)->value = NULL;
	return 0;
}

void iniFreeContext(IniContext *pContext)
{
	if (pContext == NULL)
	{
		return;
	}

	if (pContext->global.items != NULL)
	{
		free(pContext->global.items);
		memset(&pContext->global, 0, sizeof(IniSection));
	}

	hash_walk(&pContext->sections, iniFreeHashData, NULL);
	hash_destroy(&pContext->sections);
}


#define INI_FIND_ITEM(szSectionName, szItemName, pContext, pSection, \
			targetItem, pItem, return_val) \
	if (szSectionName == NULL || *szSectionName == '\0') \
	{ \
		pSection = &pContext->global; \
	} \
	else \
	{ \
		pSection = (IniSection *)hash_find(&pContext->sections, \
				szSectionName, strlen(szSectionName)); \
		if (pSection == NULL) \
		{ \
			return return_val; \
		} \
	} \
	\
	if (pSection->count <= 0) \
	{ \
		return return_val; \
	} \
	\
	snprintf(targetItem.name, sizeof(targetItem.name), "%s", szItemName); \
	pItem = (IniItem *)bsearch(&targetItem, pSection->items, \
			pSection->count, sizeof(IniItem), iniCompareByItemName);


char *iniGetStrValue(const char *szSectionName, const char *szItemName, \
		IniContext *pContext)
{
	IniItem targetItem;
	IniSection *pSection;
	IniItem *pItem;

	INI_FIND_ITEM(szSectionName, szItemName, pContext, pSection, \
			targetItem, pItem, NULL)

	if (pItem == NULL)
	{
		return NULL;
	}
	else
	{
		return pItem->value;
	}
}

int64_t iniGetInt64Value(const char *szSectionName, const char *szItemName, \
		IniContext *pContext, const int64_t nDefaultValue)
{
	char *pValue;
	
	pValue = iniGetStrValue(szSectionName, szItemName, pContext);
	if (pValue == NULL)
	{
		return nDefaultValue;
	}
	else
	{
		return strtoll(pValue, NULL, 10);
	}
}

int iniGetIntValue(const char *szSectionName, const char *szItemName, \
		IniContext *pContext, const int nDefaultValue)
{
	char *pValue;
	
	pValue = iniGetStrValue(szSectionName, szItemName, pContext);
	if (pValue == NULL)
	{
		return nDefaultValue;
	}
	else
	{
		return atoi(pValue);
	}
}

double iniGetDoubleValue(const char *szSectionName, const char *szItemName, \
		IniContext *pContext, const double dbDefaultValue)
{
	char *pValue;
	
	pValue = iniGetStrValue(szSectionName, szItemName, pContext);
	if (pValue == NULL)
	{
		return dbDefaultValue;
	}
	else
	{
		return strtod(pValue, NULL);
	}
}

bool iniGetBoolValue(const char *szSectionName, const char *szItemName, \
		IniContext *pContext, const bool bDefaultValue)
{
	char *pValue;
	
	pValue = iniGetStrValue(szSectionName, szItemName, pContext);
	if (pValue == NULL)
	{
		return bDefaultValue;
	}
	else
	{
		return  strcasecmp(pValue, "true") == 0 ||
			strcasecmp(pValue, "yes") == 0 ||
			strcasecmp(pValue, "on") == 0 ||
			strcmp(pValue, "1") == 0;
	}
}

int iniGetValues(const char *szSectionName, const char *szItemName, \
		IniContext *pContext, char **szValues, const int max_values)
{
	IniItem targetItem;
	IniSection *pSection;
	IniItem *pFound;
	IniItem *pItem;
	IniItem *pItemEnd;
	char **ppValues;

	if (max_values <= 0)
	{
		return 0;
	}
	
	INI_FIND_ITEM(szSectionName, szItemName, pContext, pSection, \
			targetItem, pFound, 0)
	if (pFound == NULL)
	{
		return 0;
	}

	ppValues = szValues;
	*ppValues++ = pFound->value;
	for (pItem=pFound-1; pItem>=pSection->items; pItem--)
	{
		if (strcmp(pItem->name, szItemName) != 0)
		{
			break;
		}

		if (ppValues - szValues < max_values)
		{
			*ppValues++ = pItem->value;
		}
	}

	pItemEnd = pSection->items + pSection->count;
	for (pItem=pFound+1; pItem<pItemEnd; pItem++)
	{
		if (strcmp(pItem->name, szItemName) != 0)
		{
			break;
		}

		if (ppValues - szValues < max_values)
		{
			*ppValues++ = pItem->value;
		}
	}

	return ppValues - szValues;
}

IniItem *iniGetValuesEx(const char *szSectionName, const char *szItemName, \
		IniContext *pContext, int *nTargetCount)
{
	IniItem targetItem;
	IniSection *pSection;
	IniItem *pFound;
	IniItem *pItem;
	IniItem *pItemEnd;
	IniItem *pItemStart;
	
	*nTargetCount = 0;
	INI_FIND_ITEM(szSectionName, szItemName, pContext, pSection, \
			targetItem, pFound, NULL)
	if (pFound == NULL)
	{
		return NULL;
	}

	*nTargetCount = 1;
	for (pItem=pFound-1; pItem>=pSection->items; pItem--)
	{
		if (strcmp(pItem->name, szItemName) != 0)
		{
			break;
		}

		(*nTargetCount)++;
	}
	pItemStart = pFound - (*nTargetCount) + 1;

	pItemEnd = pSection->items + pSection->count;
	for (pItem=pFound+1; pItem<pItemEnd; pItem++)
	{
		if (strcmp(pItem->name, szItemName) != 0)
		{
			break;
		}

		(*nTargetCount)++;
	}

	return pItemStart;
}

static int iniPrintHashData(const int index, const HashData *data, void *args)
{
	IniSection *pSection;
	IniItem *pItem;
	IniItem *pItemEnd;
	char section_name[256];
	int section_len;
	int i;

	pSection = (IniSection *)data->value;
	if (pSection == NULL)
	{
		return 0;
	}

	section_len = data->key_len;
	if (section_len >= sizeof(section_name))
	{
		section_len = sizeof(section_name) - 1;
	}

	memcpy(section_name, data->key, section_len);
	*(section_name + section_len) = '\0';

	printf("section: %s, item count: %d\n", section_name, pSection->count);
	if (pSection->count > 0)
	{
		i = 0;
		pItemEnd = pSection->items + pSection->count;
		for (pItem=pSection->items; pItem<pItemEnd; pItem++)
		{
			printf("%d. %s=%s\n", ++i, pItem->name, pItem->value);
		}
	}
	printf("\n");

	return 0;
}

void iniPrintItems(IniContext *pContext)
{
	IniItem *pItem;
	IniItem *pItemEnd;
	int i;

	printf("global section, item count: %d\n", pContext->global.count);
	if (pContext->global.count > 0)
	{
		i = 0;
		pItemEnd = pContext->global.items + pContext->global.count;
		for (pItem=pContext->global.items; pItem<pItemEnd; pItem++)
		{
			printf("%d. %s=%s\n", ++i, pItem->name, pItem->value);
		}
	}
	printf("\n");

	hash_walk(&pContext->sections, iniPrintHashData, NULL);
}

