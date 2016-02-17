#include "tpl_internal.h"

struct tpl_util_map_entry {
	tpl_util_key_t key;
	void *data;
	tpl_free_func_t free_func;
	tpl_util_map_entry_t *next;
};

static inline int
__get_bucket_index(tpl_util_map_t *map, const tpl_util_key_t key)
{
	int key_length = 0;
	int hash;

	if (map->key_length_func) key_length = map->key_length_func(key);

	hash = map->hash_func(key, key_length);
	return hash & map->bucket_mask;
}

static inline tpl_util_map_entry_t **
__get_bucket(tpl_util_map_t *map, const tpl_util_key_t key)
{
	return &map->buckets[__get_bucket_index(map, key)];
}

static int
__int64_hash(const tpl_util_key_t key, int key_length)
{
	uint64_t _key = key.key64;

	/* Hash functions from Thomas Wang https://gist.github.com/badboy/6267743 */
	_key  = ~_key + (_key << 18);
	_key ^= _key >> 31;
	_key *= 21;
	_key ^= _key >> 11;
	_key += _key << 6;
	_key ^= _key >> 22;

	return (int)_key;;
}

static int
__int64_key_compare(const tpl_util_key_t key0, int key0_length,
		    const tpl_util_key_t key1, int key1_length)
{
	return (int)(key0.key64 - key1.key64);
}

static int
__int32_hash(const tpl_util_key_t key, int key_length)
{
	uint32_t _key = (uint32_t)key.key32;

	/* Hash functions from Thomas Wang https://gist.github.com/badboy/6267743 */
	_key  = ~_key + (_key << 15);
	_key ^= _key >> 12;
	_key += _key << 2;
	_key ^= _key >> 4;
	_key *= 2057;
	_key ^= _key >> 16;

	return (int)_key;
}

static int
__int32_key_compare(const tpl_util_key_t key0, int key0_length,
		    const tpl_util_key_t key1, int key1_length)
{
	return (int)(key0.key32 - key1.key32);
}

static int
__pointer_hash(const tpl_util_key_t key, int key_length)
{
#if INTPTR_MAX == INT32_MAX
	uint32_t _key = (uint32_t)key.ptr;

	_key  = ~_key + (_key << 15);
	_key ^= _key >> 12;
	_key += _key << 2;
	_key ^= _key >> 4;
	_key *= 2057;
	_key ^= _key >> 16;

	return (int)_key;
#elif INTPTR_MAX == INT64_MAX
	uint64_t _key = (uint64_t)key.ptr;

	_key  = ~_key + (_key << 18);
	_key ^= _key >> 31;
	_key *= 21;
	_key ^= _key >> 11;
	_key += _key << 6;
	_key ^= _key >> 22;

	return (int)_key;
#else
	#error "Not 32 or 64bit system"
#endif

	return 0;
}

static int
__pointer_key_compare(const tpl_util_key_t key0, int key0_length,
		      const tpl_util_key_t key1, int key1_length)
{
	return (int)(key0.ptr - key1.ptr);
}

void
tpl_util_map_init(tpl_util_map_t *map, int bucket_bits,
		  tpl_util_hash_func_t hash_func,
		  tpl_util_key_length_func_t key_length_func,
		  tpl_util_key_compare_func_t key_compare_func,
		  void *buckets)
{
	map->hash_func = hash_func;
	map->key_length_func = key_length_func;
	map->key_compare_func = key_compare_func;

	map->bucket_bits = bucket_bits;
	map->bucket_size = 1 << bucket_bits;
	map->bucket_mask = map->bucket_size - 1;

	map->buckets = buckets;
}

void
tpl_util_map_int32_init(tpl_util_map_t *map, int bucket_bits, void *buckets)
{
	tpl_util_map_init(map, bucket_bits, __int32_hash, NULL,
		__int32_key_compare, buckets);
}

void
tpl_util_map_int64_init(tpl_util_map_t *map, int bucket_bits, void *buckets)
{
	tpl_util_map_init(map, bucket_bits, __int64_hash, NULL,
		__int64_key_compare, buckets);
}

void
tpl_util_map_pointer_init(tpl_util_map_t *map, int bucket_bits, void *buckets)
{
	tpl_util_map_init(map, bucket_bits, __pointer_hash, NULL,
		__pointer_key_compare, buckets);
}

void
tpl_util_map_fini(tpl_util_map_t *map)
{
	tpl_util_map_clear(map);
}

tpl_util_map_t *
tpl_util_map_create(int bucket_bits, tpl_util_hash_func_t hash_func,
		    tpl_util_key_length_func_t  key_length_func,
		    tpl_util_key_compare_func_t key_compare_func)
{
	tpl_util_map_t *map;
	int bucket_size = 1 << bucket_bits;

	map = calloc(1,
		sizeof(tpl_util_map_t) + bucket_size * sizeof(tpl_util_map_entry_t *));
	TPL_CHECK_ON_FALSE_RETURN_VAL(map, NULL);

	tpl_util_map_init(map, bucket_bits, hash_func, key_length_func,
		key_compare_func, map + 1);

	return map;
}

tpl_util_map_t *
tpl_util_map_int32_create(int bucket_bits)
{
	return tpl_util_map_create(bucket_bits, __int32_hash, NULL,
		__int32_key_compare);
}

tpl_util_map_t *
tpl_util_map_int64_create(int bucket_bits)
{
	return tpl_util_map_create(bucket_bits, __int64_hash, NULL,
		__int64_key_compare);
}

tpl_util_map_t *
tpl_util_map_pointer_create(int bucket_bits)
{
	return tpl_util_map_create(bucket_bits, __pointer_hash, NULL,
		__pointer_key_compare);
}

void
tpl_util_map_destroy(tpl_util_map_t *map)
{
	tpl_util_map_fini(map);
	free(map);
}

void
tpl_util_map_clear(tpl_util_map_t *map)
{
	int i;

	if (!map->buckets) return;

	for (i = 0; i < map->bucket_size; i++) {
		tpl_util_map_entry_t *curr = map->buckets[i];

		while (curr) {
			tpl_util_map_entry_t *next = curr->next;

			if (curr->free_func) curr->free_func(curr->data);

			free(curr);
			curr = next;
		}
	}

	memset(map->buckets, 0x00, map->bucket_size * sizeof(tpl_util_map_entry_t *));
}

void *
tpl_util_map_get(tpl_util_map_t *map, const tpl_util_key_t key)
{
	tpl_util_map_entry_t *curr = *__get_bucket(map, key);

	while (curr) {
		int len0 = 0;
		int len1 = 0;

		if (map->key_length_func) {
			len0 = map->key_length_func(curr->key);
			len1 = map->key_length_func(key);
		}

		if (map->key_compare_func(curr->key, len0, key, len1) == 0)
			return curr->data;

		curr = curr->next;
	}

	return NULL;
}

void
tpl_util_map_set(tpl_util_map_t *map, const tpl_util_key_t key, void *data,
		 tpl_free_func_t free_func)
{
	tpl_util_map_entry_t **bucket = __get_bucket(map, key);
	tpl_util_map_entry_t *curr = *bucket;
	tpl_util_map_entry_t *prev = NULL;
	int key_length = 0;

	/* Find existing entry for the key. */
	while (curr) {
		int len0 = 0;
		int len1 = 0;

		if (map->key_length_func) {
			len0 = map->key_length_func(curr->key);
			len1 = map->key_length_func(key);
		}

		if (map->key_compare_func(curr->key, len0, key, len1) == 0) {
			/* Free previous data. */
			if (curr->free_func) curr->free_func(curr->data);

			if (data) {
				/* Set new data. */
				curr->data = data;
				curr->free_func = free_func;
			} else {
				/* Delete entry. */
				if (prev) prev->next = curr->next;
				else *bucket = curr->next;

				free(curr);
			}

			return;
		}

		prev = curr;
		curr = curr->next;
	}

	if (!data) {
		/* Nothing to delete. */
		return;
	}

	/* Allocate a new entry. */
	if (map->key_length_func) key_length = map->key_length_func(key);

	curr = malloc(sizeof(tpl_util_map_entry_t) + key_length);
	TPL_CHECK_ON_FALSE_RETURN(curr);

	if (key_length > 0) {
		memcpy(curr + 1, key.ptr, key_length);
		curr->key.ptr = (void *)(curr + 1);
	} else {
		curr->key = key;
	}

	curr->data = data;
	curr->free_func = free_func;

	/* Insert at the head of the bucket. */
	curr->next = *bucket;
	*bucket = curr;
}
