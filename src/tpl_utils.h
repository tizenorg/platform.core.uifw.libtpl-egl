#ifndef TPL_UTILS_H
#define TPL_UTILS_H

#include "tpl.h"
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#define TPL_MIN_REGION_RECTS	16

#define TPL_ASSERT(expr)		assert(expr)
#define TPL_INLINE			__inline__
#define TPL_IGNORE(x)			(void)x

#define TPL_DMB()			__asm__ volatile("dmb sy" : : : "memory")

#if (TTRACE_ENABLE)
#include <ttrace.h>
#define DDK_TAG				TTRACE_TAG_GRAPHICS
#define TRACEBEGIN(fmt,...)		traceBegin(DDK_TAG, fmt, ##__VA_ARGS__)
#define TRACEEND()			traceEnd(DDK_TAG)
#else
#define TRACEBEGIN(fmt,...)
#define TRACEEND()
#endif

#define TPL_DEBUG 1
#if TPL_DEBUG
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

/* 0:uninitialized, 1:initialized,no log, 2:user log */
extern unsigned int tpl_log_lvl;

#define TPL_LOG(lvl, f, x...)								\
	{										\
		if (tpl_log_lvl == 1)							\
		{}									\
		else if (tpl_log_lvl > 1)						\
		{									\
			if (tpl_log_lvl <= lvl)						\
				TPL_LOG_PRINT(f, ##x)					\
		}									\
		else									\
		{									\
			char *env = getenv("TPL_LOG_LEVEL");				\
			if (env == NULL)						\
				tpl_log_lvl = 1;					\
			else								\
				tpl_log_lvl = atoi(env);				\
											\
			if (tpl_log_lvl > 1 && tpl_log_lvl <= lvl)			\
				TPL_LOG_PRINT(f, ##x)					\
		}									\
	}

#define TPL_LOG_PRINT(fmt, args...)							\
	{										\
		printf("[\x1b[36mTPL\x1b[0m|%d:%d|\x1b[36m%s\x1b[0m|%d] " fmt "\n",	\
			getpid(), (int) syscall(SYS_gettid), __func__, __LINE__,	\
			##args);							\
	}

#define TPL_ERR(f, x...)								\
	{										\
		printf("[\x1b[31mTPL_ERR %d:%d|%s|%d\x1b[0m] " f "\n",			\
			getpid(), (int) syscall(SYS_gettid), __func__,			\
			__LINE__, ##x);							\
	}

#define TPL_WARN(f, x...)								\
	{										\
		printf("[\x1b[33mTPL_WARN %d:%d|%s|%d\x1b[0m] " f "\n",			\
			getpid(), (int) syscall(SYS_gettid), __func__,			\
			__LINE__, ##x);							\
	}

#else
#define TPL_LOG(lvl, f, x...)
#define TPL_ERR(f, x...)
#define TPL_WARN(f, x...)
#endif

#define TPL_CHECK_ON_NULL_RETURN(exp)							\
	do										\
	{										\
		if ((exp) == NULL)							\
		{									\
			TPL_ERR("%s", "check failed: " # exp " == NULL");		\
			return;								\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_NULL_RETURN_VAL(exp, val)						\
	do										\
	{										\
		if ((exp) == NULL)							\
		{									\
			TPL_ERR("%s", "check failed: " # exp " == NULL");		\
			return (val);							\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_NULL_GOTO(exp, label)						\
	do										\
	{										\
		if ((exp) == NULL)							\
		{									\
			TPL_ERR("%s", "check failed: " # exp " == NULL");		\
			goto label;							\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_TRUE_RETURN(exp)							\
	do										\
	{										\
		if (exp)								\
		{									\
			TPL_ERR("%s", "check failed: " # exp " is true");		\
			return;								\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_TRUE_RETURN_VAL(exp, val)						\
	do										\
	{										\
		if (exp)								\
		{									\
			TPL_ERR("%s", "check failed: " # exp " is true");		\
			return val;							\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_TRUE_GOTO(exp, label)						\
	do										\
	{										\
		if (exp)								\
		{									\
			TPL_ERR("%s", "check failed: " # exp " is true");		\
			goto label;							\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_FALSE_RETURN(exp)							\
	do										\
	{										\
		if (!(exp))								\
		{									\
			TPL_ERR("%s", "check failed: " # exp " is false");		\
			return;								\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_FALSE_RETURN_VAL(exp, val)						\
	do										\
	{										\
		if (!(exp))								\
		{									\
			TPL_ERR("%s", "check failed: " # exp " is false");		\
			return val;							\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_FALSE_GOTO(exp, label)						\
	do										\
	{										\
		if (!(exp))								\
		{									\
			TPL_ERR("%s", "check failed: " # exp " is false");		\
			goto label;							\
		}									\
	}										\
	while (0)

#define TPL_CHECK_ON_FALSE_ASSERT_FAIL(exp, mesg)					\
	do										\
	{										\
		if (!(exp))								\
		{									\
			TPL_ERR("%s", mesg);						\
			assert(0);							\
		}									\
	}										\
	while (0)

typedef struct _tpl_list_node	tpl_list_node_t;
typedef struct _tpl_list	tpl_list_t;
typedef struct _tpl_region	tpl_region_t;

enum _tpl_occurance
{
	TPL_FIRST,
	TPL_LAST,
	TPL_ALL
};

struct _tpl_list_node
{
	tpl_list_node_t	*prev;
	tpl_list_node_t *next;
	void		*data;
	tpl_list_t	*list;
};

struct _tpl_list
{
	tpl_list_node_t	head;
	tpl_list_node_t	tail;
	int		count;
};

/**
* num_rects: number of rects.
* rects: collection of rects where each rect is specified by 4 integers which
*	are upper left (x, y) and lower right (x, y) coordinates.
* rects_static: initial storage space for rects. will be replaced by heap heap
*	memory if num_rects exceeds TPL_MIN_REGION_RECTS.
* num_rects_allocated: number of rects currently allocated. minimum is
*	TPL_MIN_REGION_RECTS (initial value).
*/
struct _tpl_region
{
	int	num_rects;
	int	*rects;
	int	rects_static[TPL_MIN_REGION_RECTS * 4];
	int	num_rects_allocated;
};

static TPL_INLINE int
tpl_list_get_count(const tpl_list_t *list)
{
	return list->count;
}

static TPL_INLINE tpl_bool_t
tpl_list_is_empty(const tpl_list_t *list)
{
	return list->count == 0;
}

static TPL_INLINE void
tpl_list_init(tpl_list_t *list)
{
	list->head.list = list;
	list->tail.list = list;

	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;

	list->count = 0;
}

static TPL_INLINE void
tpl_list_fini(tpl_list_t *list, tpl_free_func_t func)
{
	tpl_list_node_t *node = list->head.next;

	while (node != &list->tail)
	{
		tpl_list_node_t *free_node = node;
		node = node->next;

		if (func)
			func(free_node->data);

		free(free_node);
	}

	tpl_list_init(list);
}

static TPL_INLINE tpl_list_t *
tpl_list_alloc()
{
	tpl_list_t *list;

	list = (tpl_list_t *)malloc(sizeof(tpl_list_t));
	TPL_ASSERT(list != NULL);

	tpl_list_init(list);

	return list;
}

static TPL_INLINE void
tpl_list_free(tpl_list_t *list, tpl_free_func_t func)
{
	tpl_list_fini(list, func);
	free(list);
}

static TPL_INLINE void *
tpl_list_node_get_data(const tpl_list_node_t *node)
{
	return node->data;
}

static TPL_INLINE tpl_list_node_t *
tpl_list_get_front_node(tpl_list_t *list)
{
	if (tpl_list_is_empty(list))
		return NULL;

	return list->head.next;
}

static TPL_INLINE tpl_list_node_t *
tpl_list_get_back_node(tpl_list_t *list)
{
	if (tpl_list_is_empty(list))
		return NULL;

	return list->tail.prev;
}

static TPL_INLINE tpl_list_node_t *
tpl_list_node_prev(tpl_list_node_t *node)
{
	if (node->prev != &node->list->head)
		return (tpl_list_node_t *)node->prev;

	return NULL;
}

static TPL_INLINE tpl_list_node_t *
tpl_list_node_next(tpl_list_node_t *node)
{
	if (node->next != &node->list->tail)
		return node->next;

	return NULL;
}

static TPL_INLINE void *
tpl_list_get_front(const tpl_list_t *list)
{
	if (tpl_list_is_empty(list))
		return NULL;

	return list->head.next->data;
}

static TPL_INLINE void *
tpl_list_get_back(const tpl_list_t *list)
{
	if (tpl_list_is_empty(list))
		return NULL;

	return list->tail.prev->data;
}

static TPL_INLINE void
tpl_list_remove(tpl_list_node_t *node, tpl_free_func_t func)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;

	if (func)
		func(node->data);

	node->list->count--;
	free(node);
}

static TPL_INLINE void
tpl_list_insert(tpl_list_node_t *pos, void *data)
{
	tpl_list_node_t *node = (tpl_list_node_t *)malloc(sizeof(tpl_list_node_t));
	TPL_ASSERT(node != NULL);

	node->data = data;
	node->list = pos->list;

	pos->next->prev = node;
	node->next = pos->next;

	pos->next = node;
	node->prev = pos;

	pos->list->count++;
}

static TPL_INLINE void
tpl_list_remove_data(tpl_list_t *list, void *data, int occurance, tpl_free_func_t func)
{
	tpl_list_node_t *node;

	if (occurance == TPL_FIRST)
	{
	       node = list->head.next;

	       while (node != &list->tail)
	       {
		       tpl_list_node_t *curr = node;
		       node = node->next;

		       if (curr->data == data)
		       {
			       if (func)
				       func(data);

			       tpl_list_remove(curr, func);
			       return;
		       }
	       }
	}
	else if (occurance == TPL_LAST)
	{
		node = list->tail.prev;

		while (node != &list->head)
		{
			tpl_list_node_t *curr = node;
			node = node->prev;

			if (curr->data == data)
			{
				if (func)
					func(data);

				tpl_list_remove(curr, func);
				return;
			}
		}
	}
	else if (occurance == TPL_ALL)
	{
	       node = list->head.next;

	       while (node != &list->tail)
	       {
		       tpl_list_node_t *curr = node;
		       node = node->next;

		       if (curr->data == data)
		       {
			       if (func)
				       func(data);

			       tpl_list_remove(curr, func);
		       }
	       }
	}
}

static TPL_INLINE void
tpl_list_push_front(tpl_list_t *list, void *data)
{
	tpl_list_insert(&list->head, data);
}

static TPL_INLINE void
tpl_list_push_back(tpl_list_t *list, void *data)
{
	tpl_list_insert(list->tail.prev, data);
}

static TPL_INLINE void *
tpl_list_pop_front(tpl_list_t *list, tpl_free_func_t func)
{
	void		*data;

	if (tpl_list_is_empty(list))
		return NULL;

	data = list->head.next->data;
	tpl_list_remove(list->head.next, func);

	return data;
}

static TPL_INLINE void *
tpl_list_pop_back(tpl_list_t *list, tpl_free_func_t func)
{
	void		*data;

	if (tpl_list_is_empty(list))
		return NULL;

	data = list->tail.prev->data;
	tpl_list_remove(list->tail.prev, func);

	return data;
}

/* Region functions. */
void		tpl_region_init(tpl_region_t *region);
void		tpl_region_fini(tpl_region_t *region);
tpl_region_t *	tpl_region_alloc();
void		tpl_region_free(tpl_region_t **region);
tpl_bool_t	tpl_region_is_empty(const tpl_region_t *region);
void		tpl_region_copy(tpl_region_t *dst, const tpl_region_t *src);
void		tpl_region_set_rects(tpl_region_t *region, int num_rects, const int *rects);

#endif /* TPL_UTILS_H */
