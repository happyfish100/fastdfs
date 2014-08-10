/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//fast_mblock.h

#ifndef _FAST_MBLOCK_H
#define _FAST_MBLOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "common_define.h"
#include "chain.h"

/* free node chain */ 
struct fast_mblock_node
{
	struct fast_mblock_node *next;
	char data[0];   //the data buffer
};

/* malloc chain */
struct fast_mblock_malloc
{
	struct fast_mblock_malloc *next;
};

struct fast_mblock_man
{
	struct fast_mblock_node *free_chain_head;     //free node chain
	struct fast_mblock_malloc *malloc_chain_head; //malloc chain to be freed
	int element_size;         //element size
	int alloc_elements_once;  //alloc elements once
    int total_count;          //total element count
	pthread_mutex_t lock;     //the lock for read / write free node chain
};

#define fast_mblock_to_node_ptr(data_ptr) \
        (struct fast_mblock_node *)((char *)data_ptr - ((size_t)(char *) \
                    &((struct fast_mblock_node *)0)->data))

#ifdef __cplusplus
extern "C" {
#endif

/**
mblock init
parameters:
	mblock: the mblock pointer
	element_size: element size, such as sizeof(struct xxx)
	alloc_elements_once: malloc elements once, 0 for malloc 1MB once
return error no, 0 for success, != 0 fail
*/
int fast_mblock_init(struct fast_mblock_man *mblock, const int element_size, \
		const int alloc_elements_once);

/**
mblock destroy
parameters:
	mblock: the mblock pointer
*/
void fast_mblock_destroy(struct fast_mblock_man *mblock);

/**
alloc a node from the mblock
parameters:
	mblock: the mblock pointer
return the alloced node, return NULL if fail
*/
struct fast_mblock_node *fast_mblock_alloc(struct fast_mblock_man *mblock);

/**
free a node (put a node to the mblock)
parameters:
	mblock: the mblock pointer
	pNode: the node to free
return the alloced node, return NULL if fail
*/
int fast_mblock_free(struct fast_mblock_man *mblock, \
		     struct fast_mblock_node *pNode);

/**
get node count of the mblock
parameters:
	mblock: the mblock pointer
return the free node count of the mblock, return -1 if fail
*/
int fast_mblock_free_count(struct fast_mblock_man *mblock);

#define fast_mblock_total_count(mblock) (mblock)->total_count

#ifdef __cplusplus
}
#endif

#endif

