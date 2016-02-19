#include "tpl_internal.h"

#include <string.h>

#define HASH_HEAD_BITS 8
#define NUM_OF_HEADS (1 << HASH_HEAD_BITS)

#define CALC_HASH(key)  ( (((unsigned int) (key)) * 0x9e406cb5U) >> (32 - (HASH_HEAD_BITS)) );

typedef struct tpl_hlist_head tpl_hlist_head_t;
typedef struct tpl_hlist_node tpl_hlist_node_t;

/* Prototypes for internal hash list functions */
static TPL_INLINE void __tpl_hlist_init_node(tpl_hlist_node_t *n);
static TPL_INLINE int __tpl_hlist_empty(const tpl_hlist_head_t *h);
static TPL_INLINE void __tpl_hlist_del(tpl_hlist_node_t *n);

static TPL_INLINE void
__tpl_hlist_add_head(tpl_hlist_node_t *n, tpl_hlist_head_t *h);

static TPL_INLINE void
__tpl_hlist_add_before(tpl_hlist_node_t *n, tpl_hlist_node_t *next);

static TPL_INLINE void
__tpl_hlist_add_behind(tpl_hlist_node_t *n, tpl_hlist_node_t *prev);

tpl_hlist_node_t *__tpl_hlist_get_node(tpl_hlist_t *list, size_t key);
tpl_hlist_node_t *__tpl_hlist_get_tail_node(tpl_hlist_t *list, size_t key);

struct tpl_hlist {
	tpl_hlist_head_t *heads;
};

struct tpl_hlist_head {
	tpl_hlist_node_t *first;
};

struct tpl_hlist_node {
	tpl_hlist_node_t *next;
	tpl_hlist_node_t **pprev;

	size_t key;
	void *data;
};

/* Definitions for internal hash list functions */
static TPL_INLINE void
__tpl_hlist_init_node(tpl_hlist_node_t *n)
{
	n->next = NULL;
	n->pprev = NULL;
}

static TPL_INLINE int
__tpl_hlist_empty(const tpl_hlist_head_t *h)
{
	return !h->first;
}

static TPL_INLINE void
__tpl_hlist_del(tpl_hlist_node_t *n)
{
	tpl_hlist_node_t *next = n->next;
	tpl_hlist_node_t **pprev = n->pprev;

	*pprev = next;
	if (next) next->pprev = pprev;

	n->next = NULL;
	n->pprev = NULL;
}

static TPL_INLINE void
__tpl_hlist_add_head(tpl_hlist_node_t *n, tpl_hlist_head_t *h)
{
	tpl_hlist_node_t *first = h->first;

	n->next = first;

	if (first) first->pprev = &n->next;

	h->first = n;
	n->pprev = &h->first;
}

/* next must be != NULL */
static TPL_INLINE void
__tpl_hlist_add_before(tpl_hlist_node_t *n, tpl_hlist_node_t *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}

static TPL_INLINE void
__tpl_hlist_add_behind(tpl_hlist_node_t *n, tpl_hlist_node_t *prev)
{
	n->next = prev->next;
	prev->next = n;
	n->pprev = &prev->next;

	if (n->next) n->next->pprev  = &n->next;
}

tpl_hlist_node_t *
__tpl_hlist_get_node(tpl_hlist_t *list, size_t key)
{
	tpl_hlist_node_t *pos;
	size_t hash;

	hash = CALC_HASH(key);

	for (pos = list->heads[hash].first; pos; pos = pos->next) {
		if (pos->key == key)
			return pos;
	}

	return NULL;
}

tpl_hlist_node_t *
__tpl_hlist_get_tail_node(tpl_hlist_t *list, size_t key)
{
	tpl_hlist_node_t *pos;
	size_t hash;

	hash = CALC_HASH(key);

	if (__tpl_hlist_empty(&list->heads[hash])) {
		return NULL;
	}

	/* iterate until next node is NULL */
	for (pos = list->heads[hash].first; pos->next; pos = pos->next);

	return pos;
}

/* Definitions for exposed hash list functions */
tpl_hlist_t *
__tpl_hashlist_create()
{
	tpl_hlist_t *list;

	list = (tpl_hlist_t *) malloc(sizeof(tpl_hlist_t));
	if (list == NULL)
		return NULL;

	list->heads = (tpl_hlist_head_t *) malloc(sizeof(tpl_hlist_head_t) *
			NUM_OF_HEADS);
	if (list->heads == NULL) {
		free(list);
		return NULL;
	}

	memset(list->heads, 0, sizeof(tpl_hlist_head_t) * NUM_OF_HEADS);

	return list;
}

void
__tpl_hashlist_destroy(tpl_hlist_t **list)
{
	int i;
	tpl_hlist_node_t *pos;
	tpl_hlist_node_t *next;

	if (*list == NULL) return;

	for (i = 0; i < NUM_OF_HEADS; i++) {
		if (__tpl_hlist_empty(&(*list)->heads[i]))
			continue;

		pos = (*list)->heads[i].first;

		while (pos) {
			next = pos->next;

			__tpl_hlist_del(pos);
			free(pos);

			pos = next;
		}
	}

	free((*list)->heads);
	(*list)->heads = NULL;

	free(*list);
	*list = NULL;
}

tpl_result_t
__tpl_hashlist_insert(tpl_hlist_t *list, size_t key, void *data)
{
	size_t hash;
	tpl_hlist_node_t *prev_node;
	tpl_hlist_node_t *new_node;

	if (!list) return TPL_ERROR_INVALID_PARAMETER;

	/* check if key already exists in the list */
	prev_node = __tpl_hlist_get_node(list, key);
	if (prev_node) {
		TPL_ERR("key(%d) already exists in tpl_hlist_t(%p).", key, list);
		return TPL_ERROR_INVALID_PARAMETER;
	}

	/* create new node and assign given values */
	new_node = (tpl_hlist_node_t *) malloc(sizeof(tpl_hlist_node_t));
	if (!new_node) {
		TPL_ERR("Failed to allocate new tpl_hlist_node_t.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	hash = CALC_HASH(key);
	__tpl_hlist_init_node(new_node);
	new_node->key = key;
	new_node->data = data;

	/* add new node to head */
	__tpl_hlist_add_head(new_node, &list->heads[hash]);

	return TPL_ERROR_NONE;
}

void
__tpl_hashlist_delete(tpl_hlist_t *list, size_t key)
{
	tpl_hlist_node_t *node;

	node = __tpl_hlist_get_node(list, key);
	if (!node) return;

	__tpl_hlist_del(node);
	free(node);
}

void
__tpl_hashlist_do_for_all_nodes(tpl_hlist_t *list, void (*cb_func)(void *))
{
	tpl_hlist_node_t *pos;
	size_t hidx;

	if (!cb_func) return;

	for (hidx = 0; hidx < NUM_OF_HEADS; hidx++) {
		for (pos = list->heads[hidx].first; pos; pos = pos->next) {
			cb_func(pos->data);
		}
	}
}

void *
__tpl_hashlist_lookup(tpl_hlist_t *list, size_t key)
{
	tpl_hlist_node_t *node;

	node = __tpl_hlist_get_node(list, key);
	if (!node) return NULL;

	return node->data;
}
