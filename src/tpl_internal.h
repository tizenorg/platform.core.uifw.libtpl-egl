#ifndef TPL_INTERNAL_H
#define TPL_INTERNAL_H

#include "tpl.h"
#include <stdlib.h>
#include <pthread.h>

#if defined(TPL_WINSYS_DRI2) || defined(TPL_WINSYS_DRI3)
#include <xcb/xcb.h>
#endif

#include "tpl_utils.h"

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>

#define TPL_OBJECT_LOCK(object)		__tpl_object_lock((tpl_object_t *)(object))
#define TPL_OBJECT_UNLOCK(object)	__tpl_object_unlock((tpl_object_t *)(object))

typedef struct _tpl_runtime		tpl_runtime_t;
typedef struct _tpl_display_backend	tpl_display_backend_t;
typedef struct _tpl_surface_backend	tpl_surface_backend_t;

typedef struct tpl_hlist		tpl_hlist_t;

struct _tpl_display_backend
{
	tpl_backend_type_t	type;
	void			*data;

	tpl_bool_t		(*init)(tpl_display_t *display);
	void			(*fini)(tpl_display_t *display);

	tpl_bool_t		(*bind_client_display_handle)(tpl_display_t *display, tpl_handle_t native_dpy);
	tpl_bool_t		(*unbind_client_display_handle)(tpl_display_t *display, tpl_handle_t native_dpy);

	tpl_bool_t		(*query_config)(tpl_display_t *display,
						tpl_surface_type_t surface_type, int red_bits,
						int green_bits, int blue_bits, int alpha_bits,
						int color_depth, int *native_visual_id, tpl_bool_t *is_slow);
	tpl_bool_t		(*filter_config)(tpl_display_t *display, int *visual_id, int alpha_bits);

	tpl_bool_t		(*get_window_info)(tpl_display_t *display, tpl_handle_t window,
						   int *width, int *height, tbm_format *format, int depth,int a_size);
	tpl_bool_t		(*get_pixmap_info)(tpl_display_t *display, tpl_handle_t pixmap,
						   int *width, int *height, tbm_format *format);

	tbm_surface_h		(*get_native_pixmap_buffer)(tpl_handle_t pixmap);

	void			(*flush)(tpl_display_t *display);
	void			(*wait_native)(tpl_display_t *display);

};

struct _tpl_surface_backend
{
	tpl_backend_type_t	type;
	void			*data;

	tpl_bool_t	(*init)(tpl_surface_t *surface);
	void		(*fini)(tpl_surface_t *surface);

	tpl_bool_t	(*validate)(tpl_surface_t *surface);

	tbm_surface_h	(*dequeue_buffer)(tpl_surface_t *surface);
	void		(*enqueue_buffer)(tpl_surface_t *surface, tbm_surface_h tbm_surface);
};

struct _tpl_object
{
	tpl_object_type_t	type;
	tpl_util_atomic_uint	reference;
	tpl_free_func_t		free;
	pthread_mutex_t		mutex;

	struct {
		void		*data;
		tpl_free_func_t	free;
	} user_data;
};

struct _tpl_display
{
	tpl_object_t		base;

	tpl_handle_t		native_handle;

	int			bufmgr_fd;
	tpl_display_backend_t	backend;
#if defined(TPL_WINSYS_DRI2) || defined(TPL_WINSYS_DRI3)
	xcb_connection_t	*xcb_connection;
#endif
};

struct _tpl_surface
{
	tpl_object_t			base;

	tpl_display_t			*display;
	tpl_handle_t			native_handle;
	tpl_surface_type_t		type;
	tbm_format			format;
	int				width, height;

	int				post_interval;
	tpl_region_t			damage;

	int				dump_count;
	tpl_surface_backend_t		backend;
};

/*******************************************************************************
* TPL object functions
*******************************************************************************/

/** brief check wether a TPL object is valid
 * @param object the TPL object to check
 * @return TPL_TRUE on success, TPL_FALSE on error
 */
tpl_bool_t __tpl_object_is_valid(tpl_object_t *object);

/** brief initialize a TPL object
 * @param object the TPL object to initialize
 * @param type type of the TPL object
 * @param free_func customized deallocation routine for this TPL object
 * @return TPL_TRUE on success, TPL_FALSE on error
 */
tpl_bool_t __tpl_object_init(tpl_object_t *object, tpl_object_type_t type, tpl_free_func_t free_func);

/** brief destroy a TPL object
 * @param object the TPL object to destroy
 * @return TPL_TRUE on success, TPL_FALSE on error
 * @warning this function is automatically called when the reference count reaches 0, therefore it should not be expliclity called
 */
tpl_bool_t __tpl_object_fini(tpl_object_t *object);

/** brief lock a TPL object
 * @param object the TPL object to lock
 * @return TPL_TRUE on success, TPL_FALSE on error
 */
tpl_bool_t __tpl_object_lock(tpl_object_t *object);

/** brief unlock a TPL object
 * @param object the TPL object to unlock
 */
void __tpl_object_unlock(tpl_object_t *object);

/* Display functions. */
tpl_handle_t		__tpl_display_get_native_handle(tpl_display_t *display);
void			__tpl_display_flush(tpl_display_t *display);

void			__tpl_surface_set_backend_data(tpl_surface_t *surface, void *data);
void *			__tpl_surface_get_backend_data(tpl_surface_t *surface);

/* Runtime functions. */
tpl_display_t *		__tpl_runtime_find_display(tpl_backend_type_t type, tpl_handle_t native_display);
tpl_bool_t			__tpl_runtime_add_display(tpl_display_t *display);
void			__tpl_runtime_remove_display(tpl_display_t *display);
void			__tpl_runtime_flush_all_display();

/* Backend initialization functions. */
tpl_backend_type_t __tpl_display_choose_backend(tpl_handle_t native_dpy);

tpl_bool_t __tpl_display_choose_backend_gbm(tpl_handle_t native_dpy);
tpl_bool_t __tpl_display_choose_backend_tbm(tpl_handle_t native_dpy);
tpl_bool_t __tpl_display_choose_backend_wayland(tpl_handle_t native_dpy);
tpl_bool_t __tpl_display_choose_backend_x11_dri2(tpl_handle_t native_dpy);
tpl_bool_t __tpl_display_choose_backend_x11_dri3(tpl_handle_t native_dpy);

void __tpl_display_init_backend(tpl_display_t *display, tpl_backend_type_t type);
void __tpl_surface_init_backend(tpl_surface_t *surface, tpl_backend_type_t type);

void __tpl_display_init_backend_gbm(tpl_display_backend_t *backend);
void __tpl_display_init_backend_tbm(tpl_display_backend_t *backend);
void __tpl_display_init_backend_wayland(tpl_display_backend_t *backend);
void __tpl_display_init_backend_x11_dri2(tpl_display_backend_t *backend);
void __tpl_display_init_backend_x11_dri3(tpl_display_backend_t *backend);

void __tpl_surface_init_backend_gbm(tpl_surface_backend_t *backend);
void __tpl_surface_init_backend_tbm(tpl_surface_backend_t *backend);
void __tpl_surface_init_backend_wayland(tpl_surface_backend_t *backend);
void __tpl_surface_init_backend_x11_dri2(tpl_surface_backend_t *backend);
void __tpl_surface_init_backend_x11_dri3(tpl_surface_backend_t *backend);

/* Region functions. */
void __tpl_region_init(tpl_region_t *region);
void __tpl_region_fini(tpl_region_t *region);
tpl_region_t * __tpl_region_alloc();
void __tpl_region_free(tpl_region_t **region);
tpl_bool_t __tpl_region_is_empty(const tpl_region_t *region);
tpl_bool_t __tpl_region_copy(tpl_region_t *dst, const tpl_region_t *src);
tpl_bool_t __tpl_region_set_rects(tpl_region_t *region, int num_rects, const int *rects);

/* OS related functions */
void __tpl_util_sys_yield(void);
int __tpl_util_clz(int input);

/** brief get the atomic variable's value
 * @param atom storage for the value to retrieve of
 * @return value stored on success, 0 or error
*/
int __tpl_util_atomic_get(const tpl_util_atomic_uint * const atom);

/** brief set the atomic variable's value
 * @param atom storage for the value to set of
 * @param val the value to set
 * @return TPL_TRUE on succes, TPL_FALSE on error
*/
void __tpl_util_atomic_set(tpl_util_atomic_uint * const atom, unsigned int val);
unsigned int __tpl_util_atomic_inc(tpl_util_atomic_uint * const atom );
unsigned int __tpl_util_atomic_dec(tpl_util_atomic_uint * const atom );

/* Data structure functions */
tpl_hlist_t * __tpl_hashlist_create();
void __tpl_hashlist_destroy(tpl_hlist_t **list);
tpl_bool_t __tpl_hashlist_insert(tpl_hlist_t *list, size_t key, void *data);
void __tpl_hashlist_delete(tpl_hlist_t *list, size_t key);
void __tpl_hashlist_do_for_all_nodes(tpl_hlist_t *list, void (*cb_func)(void *));
void * __tpl_hashlist_lookup(tpl_hlist_t *list, size_t key);

#endif /* TPL_INTERNAL_H */
