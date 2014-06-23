/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#ifndef _HASH_H_
#define _HASH_H_

#include <sys/types.h>
#include <pthread.h>
#include "common_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CRC32_XINIT 0xFFFFFFFF		/* initial value */
#define CRC32_XOROT 0xFFFFFFFF		/* final xor value */

typedef int (*HashFunc) (const void *key, const int key_len);

#ifdef HASH_STORE_HASH_CODE
#define HASH_CODE(pHash, hash_data)   hash_data->hash_code
#else
#define HASH_CODE(pHash, hash_data)   ((unsigned int)pHash->hash_func( \
					hash_data->key, hash_data->key_len))
#endif

#define CALC_NODE_MALLOC_BYTES(key_len, value_size) \
		sizeof(HashData) + key_len + value_size

#define FREE_HASH_DATA(pHash, hash_data) \
	pHash->item_count--; \
	pHash->bytes_used -= CALC_NODE_MALLOC_BYTES(hash_data->key_len, \
				hash_data->malloc_value_size); \
	free(hash_data);


typedef struct tagHashData
{
	int key_len;
	int value_len;
	int malloc_value_size;

#ifdef HASH_STORE_HASH_CODE
	unsigned int hash_code;
#endif

	char *value;
	struct tagHashData *next;
	char key[0];
} HashData;

typedef int64_t (*ConvertValueFunc)(const HashData *old_data, const int inc,
	char *new_value, int *new_value_len, void *arg);

typedef struct tagHashArray
{
	HashData **buckets;
	HashFunc hash_func;
	int item_count;
	unsigned int *capacity;
	double load_factor;
	int64_t max_bytes;
	int64_t bytes_used;
	bool is_malloc_capacity;
	bool is_malloc_value;
	unsigned int lock_count;
	pthread_mutex_t *locks;
} HashArray;

typedef struct tagHashStat
{
	unsigned int capacity;
	int item_count;
	int bucket_used;
	double bucket_avg_length;
	int bucket_max_length;
} HashStat;

/**
 * hash walk function
 * parameters:
 *         index: item index based 0
 *         data: hash data, including key and value
 *         args: passed by hash_walk function
 * return 0 for success, != 0 for error
*/
typedef int (*HashWalkFunc)(const int index, const HashData *data, void *args);

#define hash_init(pHash, hash_func, capacity, load_factor) \
	hash_init_ex(pHash, hash_func, capacity, load_factor, 0, false)

#define hash_insert(pHash, key, key_len, value) \
	hash_insert_ex(pHash, key, key_len, value, 0, true)

/**
 * hash init function
 * parameters:
 *         pHash: the hash table
 *         hash_func: hash function
 *         capacity: init capacity
 *         load_factor: hash load factor, such as 0.75
 *         max_bytes:  max memory can be used (bytes)
 *         bMallocValue: if need malloc value buffer
 * return 0 for success, != 0 for error
*/
int hash_init_ex(HashArray *pHash, HashFunc hash_func, \
		const unsigned int capacity, const double load_factor, \
		const int64_t max_bytes, const bool bMallocValue);

/**
 * set hash locks function
 * parameters:
 *         lock_count: the lock count
 * return 0 for success, != 0 for error
*/
int hash_set_locks(HashArray *pHash, const int lock_count);

/**
 * convert the value
 * parameters:
 *         HashData: the old hash data
 *         inc: the increasement value
 *         new_value: return the new value
 *         new_value_len: return the length of the new value
 *         arg: the user data
 * return the number after increasement
*/
int64_t hash_inc_value(const HashData *old_data, const int inc,
	char *new_value, int *new_value_len, void *arg);

#define hash_inc(pHash, key, key_len, inc, value, value_len) \
	hash_inc_ex(pHash, key, key_len, inc, value, value_len, \
		hash_inc_value, NULL)

/**
 * atomic increase value
 * parameters:
 *         pHash: the hash table
 *         key: the key to insert
 *         key_len: length of th key 
 *         inc: the increasement value
 *         value: return the new value
 *         value_len: return the length of the new value
 *         convert_func: the convert function
 *         arg: the arg to convert function
 * return  0 for success, != 0 for error (errno)
 *
*/
int  hash_inc_ex(HashArray *pHash, const void *key, const int key_len,
		const int inc, char *value, int *value_len,
		ConvertValueFunc convert_func, void *arg);

/**
 * hash destroy function
 * parameters:
 *         pHash: the hash table
 * return none
*/
void hash_destroy(HashArray *pHash);

/**
 * hash insert key
 * parameters:
 *         pHash: the hash table
 *         key: the key to insert
 *         key_len: length of th key 
 *         value: the value
 *         value_len: length of the value
 *         needLock: if need lock
 * return >= 0 for success, 0 for key already exist (update), 
 *        1 for new key (insert), < 0 for error
*/
int hash_insert_ex(HashArray *pHash, const void *key, const int key_len, \
		void *value, const int value_len, const bool needLock);

/**
 * hash find key
 * parameters:
 *         pHash: the hash table
 *         key: the key to find
 *         key_len: length of th key 
 * return user data, return NULL when the key not exist
*/
void *hash_find(HashArray *pHash, const void *key, const int key_len);

/**
 * hash find key
 * parameters:
 *         pHash: the hash table
 *         key: the key to find
 *         key_len: length of th key 
 * return hash data, return NULL when the key not exist
*/
HashData *hash_find_ex(HashArray *pHash, const void *key, const int key_len);


/**
 * hash get the value of the key
 * parameters:
 *         pHash: the hash table
 *         key: the key to find
 *         key_len: length of th key
 *         value: store the value
 *         value_len: input for the max size of the value
 *                    output for the length fo the value
 * return 0 for success, != 0 fail (errno)
*/
int hash_get(HashArray *pHash, const void *key, const int key_len,
	void *value, int *value_len);


/**
 * hash partial set 
 * parameters:
 *         pHash: the hash table
 *         key: the key to insert
 *         key_len: length of th key 
 *         value: the value
 *         offset: the offset of existed value
 *         value_len: length of the value
 * return 0 for success, != 0 fail (errno)
*/
int hash_partial_set(HashArray *pHash, const void *key, const int key_len,
		const char *value, const int offset, const int value_len);

/**
 * hash delete key
 * parameters:
 *         pHash: the hash table
 *         key: the key to delete
 *         key_len: length of th key 
 * return 0 for success, != 0 fail (errno)
*/
int hash_delete(HashArray *pHash, const void *key, const int key_len);

/**
 * hash walk (iterator)
 * parameters:
 *         pHash: the hash table
 *         walkFunc: walk (interator) function
 *         args: extra args which will be passed to walkFunc
 * return 0 for success, != 0 fail (errno)
*/
int hash_walk(HashArray *pHash, HashWalkFunc walkFunc, void *args);

/**
 * get hash item count
 * parameters:
 *         pHash: the hash table
 * return item count
*/
int hash_count(HashArray *pHash);

/**
 * hash best optimize
 * parameters:
 *         pHash: the hash table
 *         suggest_capacity: suggest init capacity for speed
 * return >0 for success, < 0 fail (errno)
*/
int hash_best_op(HashArray *pHash, const int suggest_capacity);

/**
 * hash stat
 * parameters:
 *         pHash: the hash table
 *         pStat: return stat info
 *         stat_by_lens: return stats array by bucket length
 *              stat_by_lens[0] empty buckets count
 *              stat_by_lens[1] contain 1 key buckets count
 *              stat_by_lens[2] contain 2 key buckets count, etc
 *         stat_size: stats array size (contain max elments)
 * return 0 for success, != 0 fail (errno)
*/
int hash_stat(HashArray *pHash, HashStat *pStat, \
		int *stat_by_lens, const int stat_size);

/**
 * print hash stat info
 * parameters:
 *         pHash: the hash table
 * return none
*/
void hash_stat_print(HashArray *pHash);

/**
 * lock the bucket of hash table
 * parameters:
 *         pHash: the hash table
 *         bucket_index: the index of bucket
 * return 0 for success, != 0 fail (errno)
*/
int hash_bucket_lock(HashArray *pHash, const unsigned int bucket_index);

/**
 * unlock the bucket of hash table
 * parameters:
 *         pHash: the hash table
 *         bucket_index: the index of bucket
 * return 0 for success, != 0 fail (errno)
*/
int hash_bucket_unlock(HashArray *pHash, const unsigned int bucket_index);

int RSHash(const void *key, const int key_len);

int JSHash(const void *key, const int key_len);
int JSHash_ex(const void *key, const int key_len, \
	const int init_value);

int PJWHash(const void *key, const int key_len);
int PJWHash_ex(const void *key, const int key_len, \
	const int init_value);

int ELFHash(const void *key, const int key_len);
int ELFHash_ex(const void *key, const int key_len, \
	const int init_value);

int BKDRHash(const void *key, const int key_len);
int BKDRHash_ex(const void *key, const int key_len, \
	const int init_value);

int SDBMHash(const void *key, const int key_len);
int SDBMHash_ex(const void *key, const int key_len, \
	const int init_value);

int Time33Hash(const void *key, const int key_len);
int Time33Hash_ex(const void *key, const int key_len, \
	const int init_value);

int DJBHash(const void *key, const int key_len);
int DJBHash_ex(const void *key, const int key_len, \
	const int init_value);

int APHash(const void *key, const int key_len);
int APHash_ex(const void *key, const int key_len, \
	const int init_value);

int calc_hashnr (const void* key, const int key_len);

int calc_hashnr1(const void* key, const int key_len);
int calc_hashnr1_ex(const void* key, const int key_len, \
	const int init_value);

int simple_hash(const void* key, const int key_len);
int simple_hash_ex(const void* key, const int key_len, \
	const int init_value);

int CRC32(void *key, const int key_len);
int CRC32_ex(void *key, const int key_len, \
	const int init_value);

#define CRC32_FINAL(crc)  (crc ^ CRC32_XOROT)

#define INIT_HASH_CODES4(hash_codes) \
	hash_codes[0] = CRC32_XINIT; \
	hash_codes[1] = 0; \
	hash_codes[2] = 0; \
	hash_codes[3] = 0; \

#define CALC_HASH_CODES4(buff, buff_len, hash_codes) \
	hash_codes[0] = CRC32_ex(buff, buff_len, hash_codes[0]); \
	hash_codes[1] = ELFHash_ex(buff, buff_len, hash_codes[1]); \
	hash_codes[2] = simple_hash_ex(buff, buff_len, hash_codes[2]); \
	hash_codes[3] = Time33Hash_ex(buff, buff_len, hash_codes[3]); \


#define FINISH_HASH_CODES4(hash_codes) \
	hash_codes[0] = CRC32_FINAL(hash_codes[0]); \


#ifdef __cplusplus
}
#endif

#endif

