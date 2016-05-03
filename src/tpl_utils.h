#ifndef TPL_UTILS_H
#define TPL_UTILS_H

#include "tpl.h"
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#if defined(__GNUC__) && __GNUC__ >= 4
#   define TPL_API __attribute__ ((visibility("default")))
#else
#   define TPL_API
#endif
#define TPL_ASSERT(expr) assert(expr)
#define TPL_INLINE __inline__
#define TPL_IGNORE(x) (void)x

#ifdef ARM_ATOMIC_OPERATION
#define TPL_DMB() __asm__ volatile("dmb sy" : : : "memory")
#else
#define TPL_DMB() __asm__ volatile("" : : : "memory")
#endif

#if (TTRACE_ENABLE)
#include <ttrace.h>
#define TRACE_BEGIN(name,...) traceBegin(TTRACE_TAG_GRAPHICS, name, ##__VA_ARGS__)
#define TRACE_END() traceEnd(TTRACE_TAG_GRAPHICS)
#define TRACE_ASYNC_BEGIN(key, name,...) traceAsyncBegin(TTRACE_TAG_GRAPHICS, key, name, ##__VA_ARGS__)
#define TRACE_ASYNC_END(key, name,...) traceAsyncEnd(TTRACE_TAG_GRAPHICS, key, name, ##__VA_ARGS__)
#define TRACE_COUNTER(value, name,...) traceCounter(TTRACE_TAG_GRAPHICS, value, name, ##__VA_ARGS__)
#define TRACE_MARK(name,...) traceMark(TTRACE_TAG_GRAPHICS, name, ##__VA_ARGS__)
#else
#define TRACE_BEGIN(name,...)
#define TRACE_END()
#define TRACE_ASYNC_BEGIN(key, name,...)
#define TRACE_ASYNC_END(key, name,...)
#define TRACE_COUNTER(value, name,...)
#define TRACE_MARK(name,...)
#endif

#ifndef NDEBUG
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#define LOG_TAG "TPL"
#include <dlog.h>

#ifdef PNG_DUMP_ENABLE
#include <png.h>
#endif

/* 0:uninitialized, 1:initialized,no log, 2:user log */
extern unsigned int tpl_log_lvl;
extern unsigned int tpl_dump_lvl;

#ifdef DLOG_DEFAULT_ENABLE
#define TPL_LOG(lvl, f, x...) TPL_LOG_PRINT(f, ##x)
#else
#define TPL_LOG(lvl, f, x...)								\
	{										\
		if (tpl_log_lvl == lvl)							\
		{									\
			TPL_LOG_PRINT(f, ##x)						\
		}									\
		else if (tpl_log_lvl > 1 && tpl_log_lvl <=5 )				\
		{									\
			if (tpl_log_lvl <= lvl)						\
				TPL_LOG_PRINT(f, ##x)					\
		}									\
		else if (tpl_log_lvl > 5)						\
		{									\
			if (tpl_log_lvl == lvl)						\
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
			if (tpl_log_lvl > 1 && tpl_log_lvl <= 5)			\
			{								\
				if (tpl_log_lvl <= lvl)					\
					TPL_LOG_PRINT(f, ##x)				\
			} else if (tpl_log_lvl > 5) {					\
				if (tpl_log_lvl == lvl)					\
					TPL_LOG_PRINT(f, ##x)				\
			}								\
		}									\
	}
#endif
#define TPL_LOG_PRINT(fmt, args...)							\
	{										\
		LOGE("[\x1b[36mTPL\x1b[0m] \x1b[36m" fmt "\x1b[0m\n", ##args);		\
	}

#define TPL_ERR(f, x...)								\
	{										\
		LOGE("[\x1b[31mTPL_ERR\x1b[0m] \x1b[31m" f "\x1b[0m\n", ##x);		\
	}

#define TPL_WARN(f, x...)								\
	{										\
		LOGW("[\x1b[33mTPL_WARN\x1b[0m] \x1b[33m" f "\x1b[0m\n", ##x);		\
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

#define TPL_IMAGE_DUMP(data, width, height, num)					\
	{										\
		if (tpl_dump_lvl != 0)							\
		{									\
			__tpl_util_image_dump(__func__, data, tpl_dump_lvl, width, height, num);	\
		}									\
		else									\
		{									\
			char *env = getenv("TPL_DUMP_LEVEL");				\
			if (env == NULL)						\
				tpl_dump_lvl = 0;					\
			else								\
				tpl_dump_lvl = atoi(env);				\
											\
			if (tpl_dump_lvl != 0)						\
				__tpl_util_image_dump(__func__, data, tpl_dump_lvl, width, height, num);\
		}									\
	}



typedef struct _tpl_list_node	tpl_list_node_t;
typedef struct _tpl_list	tpl_list_t;
typedef struct tpl_util_map_entry tpl_util_map_entry_t;
typedef struct tpl_util_map tpl_util_map_t;
typedef union tpl_util_key tpl_util_key_t;

typedef int (*tpl_util_hash_func_t)(const tpl_util_key_t key, int key_length);
typedef int (*tpl_util_key_length_func_t)(const tpl_util_key_t key);
typedef int (*tpl_util_key_compare_func_t)(const tpl_util_key_t key0,
		int key0_length,
		const tpl_util_key_t key1,
		int key1_length);

enum _tpl_occurrence {
	TPL_FIRST,
	TPL_LAST,
	TPL_ALL
};

union tpl_util_key {
	uint32_t key32;
	uint64_t key64;
	void *ptr; /*pointer key or user defined key(string)*/
};

struct _tpl_list_node {
	tpl_list_node_t *prev;
	tpl_list_node_t *next;
	void *data;
	tpl_list_t *list;
};

struct _tpl_list {
	tpl_list_node_t head;
	tpl_list_node_t tail;
	int count;
};

struct tpl_util_map {
	tpl_util_hash_func_t hash_func;
	tpl_util_key_length_func_t key_length_func;
	tpl_util_key_compare_func_t key_compare_func;
	int bucket_bits;
	int bucket_size;
	int bucket_mask;
	tpl_util_map_entry_t **buckets;
};

void tpl_util_map_init(tpl_util_map_t *map, int bucket_bits,
		       tpl_util_hash_func_t hash_func,
		       tpl_util_key_length_func_t key_length_func,
		       tpl_util_key_compare_func_t key_compare_func,
		       void *buckets);

void tpl_util_map_int32_init(tpl_util_map_t *map, int bucket_bits,
			     void *buckets);

void tpl_util_map_int64_init(tpl_util_map_t *map, int bucket_bits,
			     void *buckets);

void tpl_util_map_pointer_init(tpl_util_map_t *map, int bucket_bits,
			       void *buckets);

void tpl_util_map_fini(tpl_util_map_t *map);

tpl_util_map_t *
tpl_util_map_create(int bucket_bits, tpl_util_hash_func_t hash_func,
		    tpl_util_key_length_func_t key_length_func,
		    tpl_util_key_compare_func_t key_compare_func);

tpl_util_map_t *tpl_util_map_int32_create(int bucket_bits);

tpl_util_map_t *tpl_util_map_int64_create(int bucket_bits);

tpl_util_map_t *tpl_util_map_pointer_create(int bucket_bits);

void tpl_util_map_destroy(tpl_util_map_t *map);

void tpl_util_map_clear(tpl_util_map_t *map);

void *tpl_util_map_get(tpl_util_map_t *map, const tpl_util_key_t key);

void
tpl_util_map_set(tpl_util_map_t *map, const tpl_util_key_t key, void *data,
		 tpl_free_func_t free_func);

static TPL_INLINE int
__tpl_list_get_count(const tpl_list_t *list)
{
	TPL_ASSERT(list);

	return list->count;
}

static TPL_INLINE tpl_bool_t
__tpl_list_is_empty(const tpl_list_t *list)
{
	TPL_ASSERT(list);

	return list->count == 0;
}

static TPL_INLINE void
__tpl_list_init(tpl_list_t *list)
{
	TPL_ASSERT(list);

	list->head.list = list;
	list->tail.list = list;

	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;

	list->count = 0;
}

static TPL_INLINE void
__tpl_list_fini(tpl_list_t *list, tpl_free_func_t func)
{
	tpl_list_node_t *node;

	TPL_ASSERT(list);

	node = list->head.next;

	while (node != &list->tail) {
		tpl_list_node_t *free_node = node;
		node = node->next;

		TPL_ASSERT(free_node);

		if (func) func(free_node->data);

		free(free_node);
	}

	__tpl_list_init(list);
}

static TPL_INLINE tpl_list_t *
__tpl_list_alloc()
{
	tpl_list_t *list;

	list = (tpl_list_t *) malloc(sizeof(tpl_list_t));
	if (!list) return NULL;

	__tpl_list_init(list);

	return list;
}

static TPL_INLINE void
__tpl_list_free(tpl_list_t *list, tpl_free_func_t func)
{
	TPL_ASSERT(list);

	__tpl_list_fini(list, func);
	free(list);
}

static TPL_INLINE void *
__tpl_list_node_get_data(const tpl_list_node_t *node)
{
	TPL_ASSERT(node);

	return node->data;
}

static TPL_INLINE tpl_list_node_t *
__tpl_list_get_front_node(tpl_list_t *list)
{
	TPL_ASSERT(list);

	if (__tpl_list_is_empty(list)) return NULL;

	return list->head.next;
}

static TPL_INLINE tpl_list_node_t *
__tpl_list_get_back_node(tpl_list_t *list)
{
	TPL_ASSERT(list);

	if (__tpl_list_is_empty(list)) return NULL;

	return list->tail.prev;
}

static TPL_INLINE tpl_list_node_t *
__tpl_list_node_prev(tpl_list_node_t *node)
{
	TPL_ASSERT(node);
	TPL_ASSERT(node->list);

	if (node->prev != &node->list->head)
		return (tpl_list_node_t *)node->prev;

	return NULL;
}

static TPL_INLINE tpl_list_node_t *
__tpl_list_node_next(tpl_list_node_t *node)
{
	TPL_ASSERT(node);
	TPL_ASSERT(node->list);

	if (node->next != &node->list->tail)
		return node->next;

	return NULL;
}

static TPL_INLINE void *
__tpl_list_get_front(const tpl_list_t *list)
{
	TPL_ASSERT(list);

	if (__tpl_list_is_empty(list))
		return NULL;

	TPL_ASSERT(list->head.next);

	return list->head.next->data;
}

static TPL_INLINE void *
__tpl_list_get_back(const tpl_list_t *list)
{
	TPL_ASSERT(list);

	if (__tpl_list_is_empty(list)) return NULL;

	TPL_ASSERT(list->tail.prev);

	return list->tail.prev->data;
}

static TPL_INLINE void
__tpl_list_remove(tpl_list_node_t *node, tpl_free_func_t func)
{
	TPL_ASSERT(node);
	TPL_ASSERT(node->prev);
	TPL_ASSERT(node->next);

	node->prev->next = node->next;
	node->next->prev = node->prev;

	if (func) func(node->data);

	node->list->count--;
	free(node);
}

static TPL_INLINE tpl_result_t
__tpl_list_insert(tpl_list_node_t *pos, void *data)
{
	tpl_list_node_t *node = (tpl_list_node_t *)malloc(sizeof(tpl_list_node_t));
	if (!node) {
		TPL_ERR("Failed to allocate new tpl_list_node_t.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	node->data = data;
	node->list = pos->list;

	pos->next->prev = node;
	node->next = pos->next;

	pos->next = node;
	node->prev = pos;

	pos->list->count++;

	return TPL_ERROR_NONE;
}

static TPL_INLINE void
__tpl_list_remove_data(tpl_list_t *list, void *data, int occurrence,
		       tpl_free_func_t func)
{
	tpl_list_node_t *node;

	TPL_ASSERT(list);

	if (occurrence == TPL_FIRST) {
		node = list->head.next;

		while (node != &list->tail) {
			tpl_list_node_t *curr;

			curr = node;
			node = node->next;

			TPL_ASSERT(curr);
			TPL_ASSERT(node);

			if (curr->data == data) {
				if (func) func(data);

				__tpl_list_remove(curr, func);
				return;
			}
		}
	} else if (occurrence == TPL_LAST) {
		node = list->tail.prev;

		while (node != &list->head) {
			tpl_list_node_t *curr;

			curr = node;
			node = node->prev;

			TPL_ASSERT(curr);
			TPL_ASSERT(node);

			if (curr->data == data) {
				if (func) func(data);

				__tpl_list_remove(curr, func);
				return;
			}
		}
	} else if (occurrence == TPL_ALL) {
		node = list->head.next;

		while (node != &list->tail) {
			tpl_list_node_t *curr;

			curr = node;
			node = node->next;

			TPL_ASSERT(curr);
			TPL_ASSERT(node);

			if (curr->data == data) {
				if (func) func(data);

				__tpl_list_remove(curr, func);
			}
		}
	}
}

static TPL_INLINE void
__tpl_list_push_front(tpl_list_t *list, void *data)
{
	TPL_ASSERT(list);

	__tpl_list_insert(&list->head, data);
}

static TPL_INLINE tpl_result_t
__tpl_list_push_back(tpl_list_t *list, void *data)
{
	TPL_ASSERT(list);

	return __tpl_list_insert(list->tail.prev, data);
}

static TPL_INLINE void *
__tpl_list_pop_front(tpl_list_t *list, tpl_free_func_t func)
{
	void *data;

	TPL_ASSERT(list);

	if (__tpl_list_is_empty(list)) return NULL;

	data = list->head.next->data;
	__tpl_list_remove(list->head.next, func);

	return data;
}

static TPL_INLINE void *
tpl_list_pop_back(tpl_list_t *list, tpl_free_func_t func)
{
	void *data;

	TPL_ASSERT(list);

	if (__tpl_list_is_empty(list)) return NULL;

	data = list->tail.prev->data;
	__tpl_list_remove(list->tail.prev, func);

	return data;
}

static TPL_INLINE int
__tpl_util_image_dump_bmp(const char *file, const void *data, int width,
			  int height)
{
	int i;

	struct {
		unsigned char magic[2];
	} bmpfile_magic = { {'B', 'M'} };

	struct {
		unsigned int filesz;
		unsigned short creator1;
		unsigned short creator2;
		unsigned int bmp_offset;
	} bmpfile_header = { 0, 0, 0, 0x36 };

	struct {
		unsigned int header_sz;
		unsigned int width;
		unsigned int height;
		unsigned short nplanes;
		unsigned short bitspp;
		unsigned int compress_type;
		unsigned int bmp_bytesz;
		unsigned int hres;
		unsigned int vres;
		unsigned int ncolors;
		unsigned int nimpcolors;
	} bmp_dib_v3_header_t = { 0x28, 0, 0, 1, 24, 0, 0, 0, 0, 0, 0 };
	unsigned int *blocks;

	if (data == NULL) return -1;

	if (width <= 0 || height <= 0) return -1;

	FILE *fp = NULL;
	if ((fp = fopen (file, "wb")) == NULL) {
		char ment[256];
		strerror_r(errno, ment, 256);
		printf("FILE ERROR:%s\t", ment);
		return -2;
	} else {
		bmpfile_header.filesz = sizeof (bmpfile_magic) + sizeof (bmpfile_header) +
					sizeof (bmp_dib_v3_header_t) + width * height * 3;
		bmp_dib_v3_header_t.header_sz = sizeof (bmp_dib_v3_header_t);
		bmp_dib_v3_header_t.width = width;
		bmp_dib_v3_header_t.height = -height;
		bmp_dib_v3_header_t.nplanes = 1;
		bmp_dib_v3_header_t.bmp_bytesz = width * height * 3;

		if (fwrite(&bmpfile_magic, sizeof (bmpfile_magic), 1, fp) < 1) {
			fclose (fp);
			return -1;
		}
		if (fwrite(&bmpfile_header, sizeof (bmpfile_header), 1, fp) < 1) {
			fclose (fp);
			return -1;
		}
		if (fwrite(&bmp_dib_v3_header_t, sizeof (bmp_dib_v3_header_t), 1, fp) < 1) {
			fclose (fp);
			return -1;
		}

		blocks = (unsigned int *)data;
		for (i = 0; i < height * width; i++) {
			if (fwrite(&blocks[i], 3, 1, fp) < 1) {
				fclose(fp);
				return -1;
			}
		}

		fclose (fp);
	}

	return 0;
}

#ifdef PNG_DUMP_ENABLE
#define PNG_DEPTH 8
static TPL_INLINE int
__tpl_util_image_dump_png(const char *file, const void *data, int width,
			  int height)
{
	TPL_CHECK_ON_FALSE_RETURN_VAL(data != NULL, -1);
	TPL_CHECK_ON_FALSE_RETURN_VAL(width > 0, -1);
	TPL_CHECK_ON_FALSE_RETURN_VAL(height > 0, -1);

	FILE *fp = fopen(file, "wb");
	int res = -2;

	if (fp) {
		res = -1;
		png_structp pPngStruct =
			png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (pPngStruct) {
			png_infop pPngInfo = png_create_info_struct(pPngStruct);

			if (pPngInfo) {
				png_init_io(pPngStruct, fp);
				png_set_IHDR(pPngStruct,
					     pPngInfo,
					     width,
					     height,
					     PNG_DEPTH,
					     PNG_COLOR_TYPE_RGBA,
					     PNG_INTERLACE_NONE,
					     PNG_COMPRESSION_TYPE_DEFAULT,
					     PNG_FILTER_TYPE_DEFAULT);

				png_set_bgr(pPngStruct);
				png_write_info(pPngStruct, pPngInfo);

				const int pixel_size = 4;       // RGBA
				png_bytep *row_pointers =
					png_malloc(pPngStruct, height * sizeof(png_byte *));
				if (!row_pointers) {
					fclose(fp);
					return res;
				}

				unsigned int *blocks = (unsigned int *) data;
				int y = 0;
				int x = 0;

				for (; y < height; ++y) {
					png_bytep row = png_malloc(pPngStruct,
								   sizeof(png_byte) * width *
								   pixel_size);
					if (!row) {
						fclose(fp);
						return res;
					}

					row_pointers[y] = (png_bytep) row;

					for (x = 0; x < width; ++x) {
						unsigned int curBlock = blocks[y * width + x];

						row[x * pixel_size] = (curBlock & 0xFF);
						row[1 + x * pixel_size] = (curBlock >> 8) & 0xFF;
						row[2 + x * pixel_size] = (curBlock >> 16) & 0xFF;
						row[3 + x * pixel_size] = (curBlock >> 24) & 0xFF;
					}
				}

				png_write_image(pPngStruct, row_pointers);
				png_write_end(pPngStruct, pPngInfo);

				for (y = 0; y < height; y++) {
					png_free(pPngStruct, row_pointers[y]);
				}
				png_free(pPngStruct, row_pointers);

				png_destroy_write_struct(&pPngStruct, &pPngInfo);

				res = 0;
			}
		}
		fclose(fp);
	}

	return res;
}
#endif

static TPL_INLINE void
__tpl_util_image_dump(const char *func, const void *data, int type,
		      int width, int height, int num)
{
	char name[200];
	char path_name[20] = "/tmp/tpl_dump";

	if (mkdir (path_name, 0755) == -1) {
		if (errno != EEXIST) {
			TPL_LOG(3, "Directory creation error!");
			return;
		}
	}

	if (type == 1) {
		snprintf(name, sizeof(name), "%s/[%d][%s][%d][%d][%04d].bmp",
			 path_name, getpid(), func, width, height, num);

		/*snprintf(name, sizeof(name), "[%d][%04d]", getpid(), num);*/
		switch (__tpl_util_image_dump_bmp(name, data, width, height)) {
		case 0:
			TPL_LOG(6, "%s file is dumped\n", name);
			break;
		case -1:
			TPL_LOG(6, "Dump failed..internal error (data = %p)(width = %d)(height = %d)\n",
				data, width, height);
			break;
		case -2:
			TPL_LOG(6, "Dump failed..file pointer error\n");
			break;
		}
	}
#ifdef PNG_DUMP_ENABLE
	else {
		snprintf(name, sizeof(name), "%s/[%d][%s][%d][%d][%04d].png",
			 path_name, getpid(), func, width, height, num);

		/*snprintf(name, sizeof(name), "[%d][%04d]", getpid(), num);*/
		switch (__tpl_util_image_dump_png(name, data, width, height)) {
		case 0:
			TPL_LOG(6, "%s file is dumped\n", name);
			break;
		case -1:
			TPL_LOG(6, "Dump failed..internal error (data = %p)(width = %d)(height = %d)\n",
				data, width, height);
			break;
		case -2:
			TPL_LOG(6, "Dump failed..file pointer error\n");
			break;
		}
	}
#endif
}
#endif /* TPL_UTILS_H */
