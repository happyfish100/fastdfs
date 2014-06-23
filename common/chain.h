/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#ifndef CHAIN_H
#define CHAIN_H

#include "common_define.h"

#define CHAIN_TYPE_INSERT	0  //insert new node before head
#define CHAIN_TYPE_APPEND	1  //insert new node after tail
#define CHAIN_TYPE_SORTED	2  //sorted chain

typedef struct tagChainNode
{
	void *data;
	struct tagChainNode *next;
} ChainNode;

typedef struct
{
	int type;
	ChainNode *head;
	ChainNode *tail;
	FreeDataFunc freeDataFunc;
	CompareFunc compareFunc;
} ChainList;

#ifdef __cplusplus
extern "C" {
#endif

/** chain init function
 *  parameters:
 *           pList: the chain list
 *           type: chain type, value is one of following:
 *           	CHAIN_TYPE_INSERT: insert new node before head
 *           	CHAIN_TYPE_APPEND: insert new node after tail 
 *           	CHAIN_TYPE_SORTED: sorted chain
 *           freeDataFunc: free data function pointer, can be NULL
 *           compareFunc: compare data function pointer, can be NULL,
 *                        must set when type is CHAIN_TYPE_SORTED
 *  return: none
 */
void chain_init(ChainList *pList, const int type, FreeDataFunc freeDataFunc, \
		CompareFunc compareFunc);

/** destroy chain
 * parameters:
 *         pList: the chain list
 * return: none
 */
void chain_destroy(ChainList *pList);


/** get the chain node count
 * parameters:
 *         pList: the chain list
 * return: chain node count
 */
int chain_count(ChainList *pList);

/** add a new node to the chain
 * parameters:
 *         pList: the chain list
 *         data: the data to add
 * return: error no, 0 for success, != 0 fail
 */
int addNode(ChainList *pList, void *data);

/** free the chain node
 * parameters:
 *         pList: the chain list
 *         pChainNode: the chain node to free
 * return: none
 */
void freeChainNode(ChainList *pList, ChainNode *pChainNode);

/** delete the chain node
 * parameters:
 *         pList: the chain list
 *         pPreviousNode: the previous chain node
 *         pDeletedNode: the chain node to delete
 * return: none
 */
void deleteNodeEx(ChainList *pList, ChainNode *pPreviousNode, \
		ChainNode *pDeletedNode);

/** delete the chain nodes from pPreviousNode->next to pDeletedNext 
 * (not including pDeletedNext)
 * parameters:
 *         pList: the chain list
 *         pPreviousNode: the previous chain node, delete from pPreviousNode->next
 *         pDeletedNext: the chain node after the deleted node
 * return: none
 */
void deleteToNodePrevious(ChainList *pList, ChainNode *pPreviousNode, \
		ChainNode *pDeletedNext);

/** delete the chain node using data compare function
 * parameters:
 *         pList: the chain list
 *         data: the first node whose data equals this will be deleted
 * return: delete chain node count, < 0 fail
 */
int deleteOne(ChainList *pList, void *data);

/** delete all chain nodes using data compare function
 * parameters:
 *         pList: the chain list
 *         data: the node whose data equals this will be deleted
 * return: delete chain node count, < 0 fail
 */
int deleteAll(ChainList *pList, void *data);

/** pop up a chain nodes from chain head
 * parameters:
 *         pList: the chain list
 * return: the head node, return NULL when the chain is empty
 */
void *chain_pop_head(ChainList *pList);

/** add a chain nodes before the chain head
 * parameters:
 *         pList: the chain list
 *         data: the node to insert
 * return: error no, 0 for success, != 0 fail
 */
int insertNodePrior(ChainList *pList, void *data);

/** add a chain nodes after the chain tail
 * parameters:
 *         pList: the chain list
 *         data: the node to insert
 * return: error no, 0 for success, != 0 fail
 */
int appendNode(ChainList *pList, void *data);

#ifdef __cplusplus
}
#endif

#endif

