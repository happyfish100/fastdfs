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
#include <errno.h>
#include "chain.h"
//#include "use_mmalloc.h"

void chain_init(ChainList *pList, const int type, FreeDataFunc freeDataFunc, \
		CompareFunc compareFunc)
{
	if (pList == NULL)
	{
		return;
	}

	pList->head = NULL;
	pList->tail = NULL;
	pList->type = type;
	pList->freeDataFunc = freeDataFunc;
	pList->compareFunc = compareFunc;

	return;
}

void chain_destroy(ChainList *pList)
{
	ChainNode *pNode;
	ChainNode *pDeleted;
	if (pList == NULL || pList->head == NULL)
	{
		return;
	}

	pNode = pList->head;
	while (pNode != NULL)
	{
		pDeleted = pNode;
		pNode = pNode->next;

		freeChainNode(pList, pDeleted);
	}

	pList->head = pList->tail = NULL;
}

void freeChainNode(ChainList *pList, ChainNode *pChainNode)
{
	if (pList->freeDataFunc != NULL)
	{
		pList->freeDataFunc(pChainNode->data);
	}

	free(pChainNode);
}

int insertNodePrior(ChainList *pList, void *data)
{
	ChainNode *pNode;
	if (pList == NULL)
	{
		return EINVAL;
	}

	pNode = (ChainNode *)malloc(sizeof(ChainNode));
	if (pNode == NULL)
	{
		return ENOMEM;
	}
	
	pNode->data = data;
	pNode->next = pList->head;

	pList->head = pNode;
	if (pList->tail == NULL)
	{
		pList->tail = pNode;
	}

	return 0;
}

int appendNode(ChainList *pList, void *data)
{
	ChainNode *pNode;
	if (pList == NULL)
	{
		return EINVAL;
	}

	pNode = (ChainNode *)malloc(sizeof(ChainNode));
	if (pNode == NULL)
	{
		return ENOMEM;
	}
	
	pNode->data = data;
	pNode->next = NULL;

	if (pList->tail != NULL)
	{
		pList->tail->next = pNode;
	}

	pList->tail = pNode;
	if (pList->head == NULL)
	{
		pList->head = pNode;
	}

	return 0;
}

int insertNodeAsc(ChainList *pList, void *data)
{
	ChainNode *pNew;
	ChainNode *pNode;
	ChainNode *pPrevious;
	if (pList == NULL || pList->compareFunc == NULL)
	{
		return EINVAL;
	}

	pNew = (ChainNode *)malloc(sizeof(ChainNode));
	if (pNew == NULL)
	{
		return ENOMEM;
	}
	
	pNew->data = data;

	pPrevious = NULL;
	pNode = pList->head;
	while (pNode != NULL && pList->compareFunc(pNode->data, data) < 0)
	{
		pPrevious = pNode;
		pNode = pNode->next;
	}

	pNew->next = pNode;
	if (pPrevious == NULL)
	{
		pList->head = pNew;
	}
	else
	{
		pPrevious->next = pNew;
	}

	if (pNode == NULL)
	{
		pList->tail = pNew;
	}

	return 0;
}

int addNode(ChainList *pList, void *data)
{
	if (pList->type == CHAIN_TYPE_INSERT)
	{
		return insertNodePrior(pList, data);
	}
	else if (pList->type == CHAIN_TYPE_APPEND)
	{
		return appendNode(pList, data);
	}
	else
	{
		return insertNodeAsc(pList, data);
	}
}

void deleteNodeEx(ChainList *pList, ChainNode *pPreviousNode, \
		ChainNode *pDeletedNode)
{
	if (pDeletedNode == pList->head)
	{
		pList->head = pDeletedNode->next;
	}
	else
	{
		pPreviousNode->next = pDeletedNode->next;
	}

	if (pDeletedNode == pList->tail)
	{
		pList->tail = pPreviousNode;
	}

	freeChainNode(pList, pDeletedNode);
}

void deleteToNodePrevious(ChainList *pList, ChainNode *pPreviousNode, \
			ChainNode *pDeletedNext)
{
	ChainNode *pNode;
	ChainNode *pDeletedNode;

	if (pPreviousNode == NULL)
	{
        	pNode = pList->head;
		pList->head = pDeletedNext;
	}
	else
	{
        	pNode = pPreviousNode->next;
		pPreviousNode->next = pDeletedNext;
	}

        while (pNode != NULL && pNode != pDeletedNext)
	{
		pDeletedNode = pNode;
		pNode = pNode->next;
	        freeChainNode(pList, pDeletedNode);
	}

	if (pDeletedNext == NULL)
	{
		pList->tail = pPreviousNode;
	}
}

void *chain_pop_head(ChainList *pList)
{
	ChainNode *pDeletedNode;
	void *data;

	pDeletedNode = pList->head; 
	if (pDeletedNode == NULL)
	{
		return NULL;
	}

	pList->head = pDeletedNode->next;
	if (pList->head == NULL)
	{
		pList->tail = NULL;
	}

	data = pDeletedNode->data;
	free(pDeletedNode);

	return data;
}

int chain_count(ChainList *pList)
{
	ChainNode *pNode;
	int count;

	count = 0;
	pNode = pList->head;
	while (pNode != NULL)
	{
		count++;
		pNode = pNode->next;
	}

	return count;
}

int deleteNode(ChainList *pList, void *data, bool bDeleteAll)
{
	ChainNode *pNode;
	ChainNode *pPrevious;
	ChainNode *pDeleted;
	int nCount;
	int nCompareRes;

	if (pList == NULL || pList->compareFunc == NULL)
	{
		return -EINVAL;
	}

	nCount = 0;
	pPrevious = NULL;
	pNode = pList->head;
	while (pNode != NULL)
	{
		nCompareRes = pList->compareFunc(pNode->data, data);
		if (nCompareRes == 0)
		{
			pDeleted = pNode;
			pNode = pNode->next;

			deleteNodeEx(pList, pPrevious, pDeleted);
			nCount++;

			if (!bDeleteAll)
			{
				break;
			}

			continue;
		}
		else if(nCompareRes > 0 && pList->type == CHAIN_TYPE_SORTED)
		{
			break;
		}

		pPrevious = pNode;
		pNode = pNode->next;
	}

	return nCount;
}

int deleteOne(ChainList *pList, void *data)
{
	return deleteNode(pList, data, false);
}

int deleteAll(ChainList *pList, void *data)
{
	return deleteNode(pList, data, true);
}

