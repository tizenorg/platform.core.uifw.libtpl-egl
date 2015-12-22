#define inline __inline__

#include <wayland-client.h>

#include <drm.h>

#include <gbm.h>
#include <gbm/gbm_tbm.h>
#include <gbm/gbm_tbmint.h>
#include <xf86drm.h>

#ifndef USE_TBM_QUEUE
#define USE_TBM_QUEUE
#endif

#undef inline

#include "tpl_internal.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tbm_surface_queue.h>
#include <wayland-tbm-client.h>
#include <wayland-tbm-server.h>

typedef struct _tpl_gbm_display       tpl_gbm_display_t;
typedef struct _tpl_gbm_surface       tpl_gbm_surface_t;
typedef struct _tpl_gbm_buffer        tpl_gbm_buffer_t;

struct _tpl_gbm_display
{
	tbm_bufmgr	bufmgr;
	tpl_list_t	cached_buffers;
	tpl_bool_t	bound_client_display;
};

struct _tpl_gbm_surface
{
	tpl_buffer_t		*current_rendering_buffer;
	tpl_list_t		done_rendering_queue;
	tbm_surface_queue_h	tbm_queue;
};

struct _tpl_gbm_buffer
{
	tpl_display_t		*display;
	tpl_buffer_t		*tpl_buffer;
	tbm_surface_h		tbm_surface;
	tbm_bo			bo;

        struct gbm_bo		*gbm_bo;
	struct wl_listener	destroy_listener;
};

#ifdef EGL_BIND_WL_DISPLAY
unsigned int __tpl_gbm_display_bind_client_wayland_display(tpl_display_t  *tpl_display,  tpl_handle_t native_dpy);
unsigned int __tpl_gbm_display_unbind_client_wayland_display(tpl_display_t  *tpl_display, tpl_handle_t native_dpy);
#endif

#define TPL_BUFFER_CACHE_MAX_ENTRIES 40

static TPL_INLINE tpl_bool_t
__tpl_gbm_surface_buffer_cache_add(tpl_list_t *buffer_cache, tpl_buffer_t *buffer)
{
	tpl_buffer_t *evict = NULL;

	TPL_ASSERT(buffer_cache);
	TPL_ASSERT(buffer);

	if (__tpl_list_get_count(buffer_cache) >= TPL_BUFFER_CACHE_MAX_ENTRIES)
	{
		evict = __tpl_list_pop_front(buffer_cache, NULL);

		TPL_ASSERT(evict);
		tpl_object_unreference(&evict->base);
	}

	TPL_LOG(3, "buf:%10p buf->base:%10p evict:%10p", buffer, &buffer->base, evict);

	if (-1 == tpl_object_reference(&buffer->base))
		return TPL_FALSE;

	return __tpl_list_push_back(buffer_cache, (void *)buffer);
}

static TPL_INLINE void
__tpl_gbm_surface_buffer_cache_remove(tpl_list_t *buffer_cache, size_t name)
{
	tpl_list_node_t *node;

	TPL_ASSERT(buffer_cache);

	node = __tpl_list_get_front_node(buffer_cache);

	while (node)
	{
		tpl_buffer_t *buffer = (tpl_buffer_t *)__tpl_list_node_get_data(node);

		TPL_ASSERT(buffer);

		if (buffer->key == name)
		{
			tpl_object_unreference(&buffer->base);
			__tpl_list_remove(node, NULL);
			TPL_LOG(3, "name:%zu buf:%10p buf->base:%10p", name, buffer, &buffer->base);
			return;
		}

		node = __tpl_list_node_next(node);
	}

	TPL_LOG(3, "Buffer named %zu not found in cache", name);
}

static TPL_INLINE tpl_buffer_t *
__tpl_gbm_surface_buffer_cache_find(tpl_list_t *buffer_cache, size_t name)
{
	tpl_list_node_t *node;

	TPL_ASSERT(buffer_cache);

	node = __tpl_list_get_front_node(buffer_cache);

	while (node)
	{
		tpl_buffer_t *buffer = (tpl_buffer_t *)__tpl_list_node_get_data(node);

		TPL_ASSERT(buffer);

		if (buffer->key == name)
		{
			TPL_LOG(3, "name:%zu buf:%10p buf->base:%10p", name, buffer, &buffer->base);
			return buffer;
		}

		node = __tpl_list_node_next(node);
	}

	TPL_LOG(3, "Buffer named %zu not found in cache", name);

	return NULL;
}


static TPL_INLINE tpl_bool_t
__tpl_gbm_display_is_gbm_device(tpl_handle_t native_dpy)
{
	TPL_ASSERT(native_dpy);

	if (*(void **)native_dpy == &wl_display_interface)
		return TPL_FALSE;

	/* MAGIC CHECK: A native display handle is a gbm_device if the de-referenced first value
	   is a memory address pointing gbm_create_surface(). */
	if (*(void **)native_dpy == gbm_create_device)
		return TPL_TRUE;

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_gbm_display_init(tpl_display_t *display)
{
	tpl_gbm_display_t *gbm_display = NULL;

	TPL_ASSERT(display);

	/* Do not allow default display in gbm. */
	if (display->native_handle == NULL)
		return TPL_FALSE;

	gbm_display = (tpl_gbm_display_t *) calloc(1, sizeof(tpl_gbm_display_t));
	if (gbm_display == NULL)
		return TPL_FALSE;

	display->backend.data = gbm_display;
	display->bufmgr_fd = -1;

	if (__tpl_gbm_display_is_gbm_device(display->native_handle))
	{
		__tpl_list_init(&gbm_display->cached_buffers);
	}
	else
		goto free_wl_display;

	return TPL_TRUE;
free_wl_display:
	if (gbm_display != NULL)
	{
		free(gbm_display);
		display->backend.data = NULL;
	}
	return TPL_FALSE;
}

static void
__tpl_gbm_display_fini(tpl_display_t *display)
{
	tpl_gbm_display_t *gbm_display;

	TPL_ASSERT(display);

	gbm_display = (tpl_gbm_display_t *)display->backend.data;
	if (gbm_display != NULL)
	{
		if (gbm_display->bound_client_display)
			__tpl_gbm_display_unbind_client_wayland_display(display, NULL);

		__tpl_list_fini(&gbm_display->cached_buffers, (tpl_free_func_t) tpl_object_unreference);

		free(gbm_display);
	}
	display->backend.data = NULL;
}

static tpl_bool_t
__tpl_gbm_display_query_config(tpl_display_t *display, tpl_surface_type_t surface_type,
				   int red_size, int green_size, int blue_size, int alpha_size,
				   int color_depth, int *native_visual_id, tpl_bool_t *is_slow)
{
	TPL_ASSERT(display);

	if (surface_type == TPL_SURFACE_TYPE_WINDOW &&
		red_size == 8 &&
		green_size == 8 &&
		blue_size == 8 &&
		(color_depth == 32 || color_depth == 24))
	{
		if (alpha_size == 8)
		{
			if (gbm_device_is_format_supported((struct gbm_device *)display->native_handle,
												GBM_FORMAT_ARGB8888,
												GBM_BO_USE_RENDERING) == 1)
			{
				if (native_visual_id != NULL) *native_visual_id = GBM_FORMAT_ARGB8888;
			}
			else
				return TPL_FALSE;

			if (is_slow != NULL) *is_slow = TPL_FALSE;
			return TPL_TRUE;
		}
		if (alpha_size == 0)
		{
			if (gbm_device_is_format_supported((struct gbm_device *)display->native_handle,
												GBM_FORMAT_XRGB8888,
												GBM_BO_USE_RENDERING) == 1)
			{
				if (native_visual_id != NULL) *native_visual_id = GBM_FORMAT_XRGB8888;
			}
			else
				return TPL_FALSE;

			if (is_slow != NULL) *is_slow = TPL_FALSE;
			return TPL_TRUE;
		}
	}

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_gbm_display_filter_config(tpl_display_t *display,
				   int *visual_id, int alpha_size)
{
	TPL_IGNORE(display);

	if (visual_id != NULL && *visual_id == GBM_FORMAT_ARGB8888 && alpha_size == 0)
	{
		*visual_id = GBM_FORMAT_XRGB8888;
		return TPL_TRUE;
	}

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_gbm_display_get_window_info(tpl_display_t *display, tpl_handle_t window,
				      int *width, int *height, tpl_format_t *format, int depth, int a_size)
{
	TPL_ASSERT(display);
	TPL_ASSERT(window);

	struct gbm_surface *gbm_surface = (struct gbm_surface *)window;
	tbm_surface_queue_h surf_queue = (tbm_surface_queue_h)gbm_tbm_get_surface_queue(gbm_surface);

	if (format != NULL)
	{
		switch (tbm_surface_queue_get_format(surf_queue))
		{
			case TBM_FORMAT_ARGB8888: *format = TPL_FORMAT_ARGB8888; break;
			case TBM_FORMAT_XRGB8888: *format = TPL_FORMAT_XRGB8888; break;
			case TBM_FORMAT_RGB565: *format = TPL_FORMAT_RGB565; break;
			default: *format = TPL_FORMAT_INVALID; break;
		}
	}
	if (width != NULL) *width = tbm_surface_queue_get_width(surf_queue);
	if (height != NULL) *height = tbm_surface_queue_get_height(surf_queue);
	return TPL_TRUE;

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_gbm_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
				      int *width, int *height, tpl_format_t *format)
{
	tbm_surface_h	tbm_surface = NULL;
	int		tbm_format = -1;

	tbm_surface = wayland_tbm_server_get_surface(NULL, (struct wl_resource*)pixmap);
	if (tbm_surface == NULL)
		return TPL_FALSE;

	if (width) *width = tbm_surface_get_width(tbm_surface);
	if (height) *height = tbm_surface_get_height(tbm_surface);
	if (format)
	{
		tbm_format = tbm_surface_get_format(tbm_surface);
		switch(tbm_format)
		{
			case TBM_FORMAT_ARGB8888: *format = TPL_FORMAT_ARGB8888; break;
			case TBM_FORMAT_XRGB8888: *format = TPL_FORMAT_XRGB8888; break;
			case TBM_FORMAT_RGB565: *format = TPL_FORMAT_RGB565; break;
			default:
				*format = TPL_FORMAT_INVALID;
				return TPL_FALSE;
		}
	}

	return TPL_TRUE;
}

static void
__tpl_gbm_display_flush(tpl_display_t *display)
{
	TPL_IGNORE(display);

	/* Do nothing. */
}

static tpl_bool_t
__tpl_gbm_surface_init(tpl_surface_t *surface)
{
	tpl_gbm_surface_t *tpl_gbm_surface = NULL;
	TPL_ASSERT(surface);

	tpl_gbm_surface = (tpl_gbm_surface_t *) calloc(1, sizeof(tpl_gbm_surface_t));
	if (NULL == tpl_gbm_surface)
		return TPL_FALSE;

	surface->backend.data = (void *)tpl_gbm_surface;
	tpl_gbm_surface->tbm_queue = NULL;

	__tpl_list_init(&tpl_gbm_surface->done_rendering_queue);

	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		struct gbm_surface *gbm_surface = (struct gbm_surface*)surface->native_handle;
		tpl_gbm_surface->tbm_queue = (tbm_surface_queue_h)gbm_tbm_get_surface_queue(gbm_surface);

		if (TPL_TRUE != __tpl_gbm_display_get_window_info(surface->display, surface->native_handle,
					&surface->width, &surface->height, NULL, 0, 0))
			goto error;

		TPL_LOG(3, "window(%p, %p) %dx%d", surface, surface->native_handle, surface->width, surface->height);
		return TPL_TRUE;
	}
	else if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		if (TPL_TRUE != __tpl_gbm_display_get_pixmap_info(surface->display, surface->native_handle,
					&surface->width, &surface->height, NULL))
			goto error;

		return TPL_TRUE;
	}

error:
	free(tpl_gbm_surface);

	return TPL_FALSE;
}

static void
__tpl_gbm_surface_buffer_free(tpl_buffer_t *buffer)
{
	TPL_LOG(3, "buffer(%p) key:%zu", buffer, buffer?buffer->key:-1);
	if (buffer != NULL)
	{
		__tpl_buffer_set_surface(buffer, NULL);
		tpl_object_unreference((tpl_object_t *) buffer);
	}
}

static tpl_bool_t
__tpl_gbm_surface_destroy_cached_buffers(tpl_surface_t *surface)
{
	tpl_gbm_surface_t *gbm_surface = NULL;
	tpl_gbm_display_t *gbm_display = NULL;

	if (surface == NULL)
	{
		TPL_ERR("tpl surface is invalid!!\n");
		return TPL_FALSE;
	}

	gbm_surface = (tpl_gbm_surface_t*)surface->backend.data;
	gbm_display = (tpl_gbm_display_t*)surface->display->backend.data;

	if (gbm_surface == NULL || gbm_display == NULL)
	{
		TPL_ERR("tpl surface has invalid members!!\n");
		return TPL_FALSE;
	}

	return TPL_TRUE;
}

static tpl_bool_t
__tpl_gbm_surface_update_cached_buffers(tpl_surface_t *surface)
{
	tpl_gbm_surface_t *gbm_surface = NULL;
	tpl_gbm_display_t *gbm_display = NULL;

	if (surface == NULL)
	{
		TPL_ERR("tpl surface is invalid!!\n");
		return TPL_FALSE;
	}

	gbm_surface = (tpl_gbm_surface_t*)surface->backend.data;
	gbm_display = (tpl_gbm_display_t*)surface->display->backend.data;

	if (gbm_surface == NULL || gbm_display == NULL)
	{
		TPL_ERR("tpl surface has invalid members!!\n");
		return TPL_FALSE;
	}

	return TPL_TRUE;
}

static void
__tpl_gbm_surface_fini(tpl_surface_t *surface)
{
	tpl_gbm_surface_t *gbm_surface = NULL;
	tpl_gbm_display_t *gbm_display = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	gbm_surface = (tpl_gbm_surface_t *) surface->backend.data;
	if (NULL == gbm_surface)
		return;

	gbm_display = (tpl_gbm_display_t *) surface->display->backend.data;
	if (NULL == gbm_display)
		return;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	free(gbm_surface);
	surface->backend.data = NULL;
}

static void
__tpl_gbm_surface_post(tpl_surface_t *surface, tpl_frame_t *frame)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);
	TPL_ASSERT(frame);
	TPL_ASSERT(frame->buffer);

	tpl_gbm_buffer_t *gbm_buffer = NULL;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	tpl_gbm_surface_t *gbm_surface = (tpl_gbm_surface_t*)surface->backend.data;
	tpl_buffer_t *buffer = NULL;

	if (!__tpl_list_is_empty(&gbm_surface->done_rendering_queue))
	{
		buffer = __tpl_list_pop_front(&gbm_surface->done_rendering_queue, NULL);

		TPL_ASSERT(buffer);

		gbm_buffer = (tpl_gbm_buffer_t *) buffer->backend.data;
	}

	tbm_surface_internal_unref(gbm_buffer->tbm_surface);

	if (gbm_surface->tbm_queue && gbm_buffer->tbm_surface)
	{
		tbm_surface_queue_enqueue(gbm_surface->tbm_queue, gbm_buffer->tbm_surface);
		TPL_LOG(6, "tbm_surface ENQUEUED!!");
	}
}

static tpl_bool_t
__tpl_gbm_surface_begin_frame(tpl_surface_t *surface)
{
	tpl_gbm_surface_t *gbm_surface;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	gbm_surface = (tpl_gbm_surface_t *) surface->backend.data;

	TPL_ASSERT(gbm_surface->current_rendering_buffer == NULL);

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	return TPL_TRUE;
}

static tpl_bool_t
__tpl_gbm_surface_validate_frame(tpl_surface_t *surface)
{
	TPL_IGNORE(surface);

	return TPL_TRUE;
}

static tpl_bool_t
__tpl_gbm_surface_end_frame(tpl_surface_t *surface)
{
	tpl_gbm_surface_t	*gbm_surface = NULL;
	tpl_gbm_buffer_t	*gbm_buffer	 = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	gbm_surface = (tpl_gbm_surface_t *) surface->backend.data;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (gbm_surface->current_rendering_buffer != NULL)
	{
		gbm_buffer = (tpl_gbm_buffer_t *) gbm_surface->current_rendering_buffer->backend.data;

		TPL_LOG(6, "current_rendering_buffer BO:%d", tbm_bo_export(gbm_buffer->bo));
	}

	/* MOVE BUFFER : (current buffer) --> [done queue] */
	if (TPL_TRUE != __tpl_list_push_back(&gbm_surface->done_rendering_queue, gbm_surface->current_rendering_buffer))
		return TPL_FALSE;

	gbm_surface->current_rendering_buffer = NULL;

	return TPL_TRUE;
}

static int
__tpl_gbm_get_depth_from_format(tpl_format_t format)
{
	int depth = 0;

	switch(format)
	{
		case TPL_FORMAT_BGR565:
		case TPL_FORMAT_RGB565:
		case TPL_FORMAT_ABGR4444:
		case TPL_FORMAT_ARGB4444:
		case TPL_FORMAT_BGRA4444:
		case TPL_FORMAT_RGBA4444:
		case TPL_FORMAT_ABGR1555:
		case TPL_FORMAT_ARGB1555:
		case TPL_FORMAT_BGRA5551:
		case TPL_FORMAT_RGBA5551:
			depth = 16;
			break;
		case TPL_FORMAT_ABGR8888:
		case TPL_FORMAT_ARGB8888:
		case TPL_FORMAT_BGRA8888:
		case TPL_FORMAT_RGBA8888:
		case TPL_FORMAT_XBGR8888:
		case TPL_FORMAT_XRGB8888:
		case TPL_FORMAT_BGRX8888:
		case TPL_FORMAT_RGBX8888:
			depth = 32;
			break;
		case TPL_FORMAT_BGR888:
		case TPL_FORMAT_RGB888:
			depth = 24;
			break;
		default:
			depth = 32;
	}

	return depth;
}

static int tpl_buffer_key;
#define KEY_TPL_BUFFER  (unsigned long)(&tpl_buffer_key)

static inline tpl_buffer_t *
__tpl_gbm_surface_get_buffer_from_tbm_surface(tbm_surface_h surface)
{
    tbm_bo bo;
    tpl_buffer_t* buf=NULL;

    bo = tbm_surface_internal_get_bo(surface, 0);
    tbm_bo_get_user_data(bo, KEY_TPL_BUFFER, (void **)&buf);

    return buf;
}

static inline void
__tpl_gbm_buffer_set_tbm_surface(tbm_surface_h surface, tpl_buffer_t *buf)
{
    tbm_bo bo;

    bo = tbm_surface_internal_get_bo(surface, 0);
    tbm_bo_add_user_data(bo, KEY_TPL_BUFFER, NULL);
    tbm_bo_set_user_data(bo, KEY_TPL_BUFFER, buf);
}

static tpl_buffer_t *
__tpl_gbm_surface_create_buffer_from_gbm_surface(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_gbm_buffer_t *gbm_buffer = NULL;
	tbm_bo bo;
	tbm_surface_h tbm_surface = NULL;
	tbm_surface_queue_error_e tsq_err = 0;

	tbm_bo_handle bo_handle;
	int width, height, depth;
	uint32_t size, offset, stride, key;
	tpl_format_t format;
	tpl_gbm_surface_t *gbm_surface = NULL;
	tpl_gbm_display_t *gbm_display = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->native_handle);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);

	gbm_surface = (tpl_gbm_surface_t*)surface->backend.data;
	gbm_display = (tpl_gbm_display_t*)surface->display->backend.data;

	tsq_err = tbm_surface_queue_dequeue(gbm_surface->tbm_queue, &tbm_surface);
	if (tbm_surface == NULL)
	{
		TPL_LOG(6, "Wait until dequeable | tsq_err = %d", tsq_err);
		tbm_surface_queue_can_dequeue(gbm_surface->tbm_queue, 1);

		tsq_err = tbm_surface_queue_dequeue(gbm_surface->tbm_queue, &tbm_surface);
		if (tbm_surface == NULL)
		{
			TPL_ERR("Failed to get tbm_surface from tbm_surface_queue | tsq_err = %d",tsq_err);
			return NULL;
		}
	}

	/* Inc ref count about tbm_surface */
	/* It will be dec when before tbm_surface_queue_enqueue called */
	tbm_surface_internal_ref(tbm_surface);

	if ((bo = tbm_surface_internal_get_bo(tbm_surface, 0)) == NULL)
	{
		TPL_ERR("Failed to get tbm_bo from tbm_surface");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	key = tbm_bo_export(bo);

	buffer = __tpl_gbm_surface_buffer_cache_find(&gbm_display->cached_buffers, key);
	if (buffer != NULL)
	{
		return buffer;
	}

	width = tbm_surface_get_width(tbm_surface);
	height = tbm_surface_get_height(tbm_surface);

	switch(tbm_surface_get_format(tbm_surface))
	{
		case TBM_FORMAT_ARGB8888: format = TPL_FORMAT_ARGB8888; break;
		case TBM_FORMAT_XRGB8888: format = TPL_FORMAT_XRGB8888; break;
		case TBM_FORMAT_RGB565: format = TPL_FORMAT_RGB565; break;
		default:
		format = TPL_FORMAT_INVALID;
		TPL_ERR("No matched format!!");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	depth = __tpl_gbm_get_depth_from_format(format);

	/* Get pitch stride from tbm_surface */
	tbm_surface_internal_get_plane_data(tbm_surface, 0, &size, &offset, &stride);

	/* Create tpl buffer. */
	bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);

	buffer = __tpl_buffer_alloc(surface, (size_t) key,
								(int)bo_handle.u32, width, height, depth, stride);

	if (buffer == NULL)
	{
		TPL_ERR("Failed to allocate tpl buffer | surf:%p bo_hnd:%d WxHxD:%dx%dx%d",
			surface, (int) bo_handle.u32, width, height, depth);
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	gbm_buffer = (tpl_gbm_buffer_t *) calloc(1, sizeof(tpl_gbm_buffer_t));
	if (gbm_buffer == NULL)
	{
		TPL_ERR("Mem alloc for gbm_buffer failed!");
		tpl_object_unreference((tpl_object_t *) buffer);
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	buffer->backend.data = (void *)gbm_buffer;

	/* Post process */
	gbm_buffer->display = surface->display;
	gbm_buffer->bo = bo;
	gbm_buffer->tbm_surface = tbm_surface;

	if (TPL_TRUE != __tpl_gbm_surface_buffer_cache_add(&gbm_display->cached_buffers, buffer))
	{
		TPL_ERR("Adding surface to buffer cache failed!");
		tpl_object_unreference((tpl_object_t *) buffer);
		tbm_surface_internal_unref(tbm_surface);
		free(gbm_buffer);
		return NULL;
	}

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "buffer:%p bo_hnd:%d, %dx%d", buffer, (int) bo_handle.u32, width, height);
	__tpl_gbm_buffer_set_tbm_surface(tbm_surface, buffer);

	return buffer;
}

static void
__tpl_gbm_buffer_destroy_notify(struct wl_listener *listener, void *data)
{
	tpl_display_t *display;
	tpl_gbm_display_t *gbm_display;
	tpl_gbm_buffer_t *gbm_buffer = NULL;
	size_t key = 0;

	gbm_buffer = wl_container_of(listener, gbm_buffer, destroy_listener);
	display = gbm_buffer->display;
	key = tbm_bo_export(gbm_buffer->bo);
	gbm_display = (tpl_gbm_display_t *)display->backend.data;
	tpl_object_unreference((tpl_object_t *)gbm_buffer->tpl_buffer);
	__tpl_gbm_surface_buffer_cache_remove(&gbm_display->cached_buffers, key);
}

static tpl_buffer_t *
__tpl_gbm_surface_create_buffer_from_wl_tbm(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_gbm_buffer_t *gbm_buffer = NULL;
	tbm_surface_h tbm_surface = NULL;
	/* TODO: If HW support getting of  gem memory size,
		use tbm_surface_get_info() with tbm_surface_info_s  */
#if 0
	tbm_surface_info_s tbm_surf_info;
#endif
	tbm_bo bo;
	tbm_bo_handle bo_handle;

	int width = 0, height = 0, depth;
	uint32_t size, offset, stride;
	tpl_format_t format = TPL_FORMAT_INVALID;
	size_t key = 0;

	tpl_gbm_display_t *gbm_display;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->native_handle);

	gbm_display = (tpl_gbm_display_t *) surface->display->backend.data;

	tbm_surface = wayland_tbm_server_get_surface(NULL, (struct wl_resource*)surface->native_handle);
	if (tbm_surface == NULL)
	{
		TPL_ERR("Failed to get tbm surface!");
		return NULL;
	}

	bo = tbm_surface_internal_get_bo(tbm_surface, 0);
	key = tbm_bo_export(bo);

	buffer = __tpl_gbm_surface_buffer_cache_find(&gbm_display->cached_buffers, key);
	if (buffer != NULL)
	{
		__tpl_buffer_set_surface(buffer, surface);
	}
	else
	{
		/* Inc ref count about tbm_surface */
		/* It will be dec when gbm_buffer_fini called*/
		tbm_surface_internal_ref(tbm_surface);

		if (TPL_TRUE != __tpl_gbm_display_get_pixmap_info(
					surface->display,
					surface->native_handle,
					&width, &height, &format))
		{
			TPL_ERR("Failed to get pixmap info!");
			tbm_surface_internal_unref(tbm_surface);
			return NULL;
		}
		/* TODO: If HW support getting of  gem memory size,
			then replace tbm_surface_internal_get_plane_data() to tbm_surface_get_info() */
#if 0
		if (tbm_surface_get_info(tbm_surface, &tbm_surf_info) != 0)
		{
			TPL_ERR("Failed to get stride info!");
			tbm_surface_internal_unref(tbm_surface);
			return NULL;
		}
		stride = tbm_surf_info.planes[0].stride;
#else
		if (!tbm_surface_internal_get_plane_data(tbm_surface, 0, &size, &offset,  &stride))
		{
			TPL_ERR("Failed to get tbm_surface stride info!");
			tbm_surface_internal_unref(tbm_surface);
			return NULL;
		}
#endif
		depth = __tpl_gbm_get_depth_from_format(format);

		/* Create tpl buffer. */
		bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);
		if (NULL == bo_handle.ptr)
		{
			TPL_ERR("Failed to get bo handle!");
			tbm_surface_internal_unref(tbm_surface);
			return NULL;
		}

		buffer = __tpl_buffer_alloc(surface, key,
									(int) bo_handle.u32, width, height, depth, stride);
		if (buffer == NULL)
		{
			TPL_ERR("Failed to alloc TPL buffer!");
			tbm_surface_internal_unref(tbm_surface);
			return NULL;
		}

		gbm_buffer = (tpl_gbm_buffer_t *) calloc(1, sizeof(tpl_gbm_buffer_t));
		if (gbm_buffer == NULL)
		{
			TPL_ERR("Mem alloc failed for gbm buffer!");
			tpl_object_unreference((tpl_object_t *) buffer);
			tbm_surface_internal_unref(tbm_surface);
			return NULL;
		}

		gbm_buffer->display = surface->display;
		gbm_buffer->bo = bo;
		gbm_buffer->tbm_surface = tbm_surface;
		gbm_buffer->tpl_buffer = buffer;

		buffer->backend.data = (void *)gbm_buffer;
		buffer->key = key;

		if (TPL_TRUE != __tpl_gbm_surface_buffer_cache_add(&gbm_display->cached_buffers, buffer))
		{
			TPL_ERR("Adding surface to buffer cache failed!");
			tpl_object_unreference((tpl_object_t *) buffer);
			tbm_surface_internal_unref(tbm_surface);
			free(gbm_buffer);
			return NULL;
		}

		gbm_buffer->destroy_listener.notify = __tpl_gbm_buffer_destroy_notify;
		wl_resource_add_destroy_listener((struct wl_resource*)surface->native_handle, &gbm_buffer->destroy_listener);
	}

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	return buffer;
}

static tpl_buffer_t *
__tpl_gbm_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	int width, height;
	tpl_gbm_surface_t *gbm_surface;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->backend.data);

	gbm_surface = (tpl_gbm_surface_t *)surface->backend.data;

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "window(%p, %p), current(%p)", surface, surface->native_handle,
		gbm_surface->current_rendering_buffer);

	if (surface->type != TPL_SURFACE_TYPE_PIXMAP &&
		TPL_TRUE != __tpl_gbm_display_get_window_info(surface->display,
		surface->native_handle, &width, &height, NULL, 0, 0))
	{
		TPL_ERR("Failed to get window info!");
		return NULL;
	}

	/* Check whether the surface was resized by wayland_egl */
	if (surface->type != TPL_SURFACE_TYPE_PIXMAP &&
		gbm_surface->current_rendering_buffer != NULL &&
		(width != gbm_surface->current_rendering_buffer->width ||
		height != gbm_surface->current_rendering_buffer->height))
	{
		__tpl_gbm_surface_buffer_free(gbm_surface->current_rendering_buffer);
		gbm_surface->current_rendering_buffer = NULL;

		if (reset_buffers != NULL)
			*reset_buffers = TPL_TRUE;
	}

	if (gbm_surface->current_rendering_buffer == NULL)
	{
		if (surface->type == TPL_SURFACE_TYPE_WINDOW)
		{
			gbm_surface->current_rendering_buffer =
				__tpl_gbm_surface_create_buffer_from_gbm_surface(surface, reset_buffers);
		}
		if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
		{
			gbm_surface->current_rendering_buffer =
				__tpl_gbm_surface_create_buffer_from_wl_tbm(surface, reset_buffers);
		}
		TPL_LOG(3, "window(%p, %p), current(%p)", surface, surface->native_handle,
				gbm_surface->current_rendering_buffer);
	}

	TPL_ASSERT(gbm_surface->current_rendering_buffer);

	return gbm_surface->current_rendering_buffer;
}

static tpl_bool_t
__tpl_gbm_buffer_init(tpl_buffer_t *buffer)
{
	TPL_IGNORE(buffer);

	return TPL_TRUE;
}

static void
__tpl_gbm_buffer_fini(tpl_buffer_t *buffer)
{
	TPL_ASSERT(buffer);

	TPL_LOG(3, "tpl_buffer(%p) key:%zu fd:%d %dx%d", buffer, buffer->key, buffer->fd, buffer->width, buffer->height);

	if (buffer->backend.data)
	{
		tpl_gbm_buffer_t *gbm_buffer = (tpl_gbm_buffer_t *)buffer->backend.data;

		if (gbm_buffer->bo != NULL && gbm_buffer->tbm_surface != NULL)
		{
			tbm_surface_internal_unref(gbm_buffer->tbm_surface);
			tbm_surface_destroy(gbm_buffer->tbm_surface);
			gbm_buffer->bo = NULL;
			gbm_buffer->tbm_surface = NULL;
		}

		buffer->backend.data = NULL;
		free(gbm_buffer);
	}
}

static void *
__tpl_gbm_buffer_map(tpl_buffer_t *buffer, int size)
{
	tpl_gbm_buffer_t *gbm_buffer;
	tbm_bo_handle handle;

	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.data);

	gbm_buffer = (tpl_gbm_buffer_t *) buffer->backend.data;

	TPL_ASSERT(gbm_buffer->bo);

	handle = tbm_bo_get_handle(gbm_buffer->bo, TBM_DEVICE_CPU);
	return handle.ptr;
}

static void
__tpl_gbm_buffer_unmap(tpl_buffer_t *buffer, void *ptr, int size)
{
	TPL_IGNORE(buffer);
	TPL_IGNORE(ptr);
	TPL_IGNORE(size);

	/* Do nothing. */
}

static tpl_bool_t
__tpl_gbm_buffer_lock(tpl_buffer_t *buffer, tpl_lock_usage_t usage)
{
	tpl_gbm_buffer_t *gbm_buffer;
	tbm_bo_handle handle;

	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.data);

	gbm_buffer = (tpl_gbm_buffer_t *) buffer->backend.data;

	TPL_ASSERT(gbm_buffer->bo);

	TPL_OBJECT_UNLOCK(buffer);

	switch (usage)
	{
		case TPL_LOCK_USAGE_GPU_READ:
			handle = tbm_bo_map(gbm_buffer->bo, TBM_DEVICE_3D, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_GPU_WRITE:
			handle = tbm_bo_map(gbm_buffer->bo, TBM_DEVICE_3D, TBM_OPTION_WRITE);
			break;
		case TPL_LOCK_USAGE_CPU_READ:
			handle = tbm_bo_map(gbm_buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_CPU_WRITE:
			handle = tbm_bo_map(gbm_buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
			break;
		default:
			TPL_ERR("Unsupported buffer usage!");
			TPL_OBJECT_LOCK(buffer);
			return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(buffer);

	if (handle.u32 != 0 || handle.ptr != NULL)
		return TPL_FALSE;

	return TPL_TRUE;
}

static void
__tpl_gbm_buffer_unlock(tpl_buffer_t *buffer)
{
	tpl_gbm_buffer_t *gbm_buffer;

	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.data);

	gbm_buffer = (tpl_gbm_buffer_t *) buffer->backend.data;

	TPL_ASSERT(gbm_buffer->bo);

	TPL_OBJECT_UNLOCK(buffer);
	tbm_bo_unmap(gbm_buffer->bo);
	TPL_OBJECT_LOCK(buffer);
}

tpl_bool_t
__tpl_display_choose_backend_gbm(tpl_handle_t native_dpy)
{
	if (native_dpy == NULL)
		return TPL_FALSE;

	if (__tpl_gbm_display_is_gbm_device(native_dpy))
		return TPL_TRUE;

	return TPL_FALSE;
}

void
__tpl_display_init_backend_gbm(tpl_display_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_GBM;
	backend->data = NULL;

	backend->init				= __tpl_gbm_display_init;
	backend->fini				= __tpl_gbm_display_fini;
	backend->query_config			= __tpl_gbm_display_query_config;
	backend->filter_config			= __tpl_gbm_display_filter_config;
	backend->get_window_info		= __tpl_gbm_display_get_window_info;
	backend->get_pixmap_info		= __tpl_gbm_display_get_pixmap_info;
	backend->flush				= __tpl_gbm_display_flush;
#ifdef EGL_BIND_WL_DISPLAY
	backend->bind_client_display_handle	= __tpl_gbm_display_bind_client_wayland_display;
	backend->unbind_client_display_handle	= __tpl_gbm_display_unbind_client_wayland_display;
#endif
}

void
__tpl_surface_init_backend_gbm(tpl_surface_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_GBM;
	backend->data = NULL;

	backend->init		= __tpl_gbm_surface_init;
	backend->fini		= __tpl_gbm_surface_fini;
	backend->begin_frame	= __tpl_gbm_surface_begin_frame;
	backend->end_frame	= __tpl_gbm_surface_end_frame;
	backend->validate_frame	= __tpl_gbm_surface_validate_frame;
	backend->get_buffer	= __tpl_gbm_surface_get_buffer;
	backend->post		= __tpl_gbm_surface_post;
	backend->destroy_cached_buffers = __tpl_gbm_surface_destroy_cached_buffers;
	backend->update_cached_buffers = __tpl_gbm_surface_update_cached_buffers;
}

void
__tpl_buffer_init_backend_gbm(tpl_buffer_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_GBM;
	backend->data = NULL;

	backend->init			= __tpl_gbm_buffer_init;
	backend->fini			= __tpl_gbm_buffer_fini;
	backend->map			= __tpl_gbm_buffer_map;
	backend->unmap			= __tpl_gbm_buffer_unmap;
	backend->lock			= __tpl_gbm_buffer_lock;
	backend->unlock			= __tpl_gbm_buffer_unlock;
	backend->create_native_buffer	= NULL;
}

#ifdef EGL_BIND_WL_DISPLAY
unsigned int __tpl_gbm_display_bind_client_wayland_display(tpl_display_t *tpl_display, tpl_handle_t native_dpy)
{
	tpl_gbm_display_t *tpl_gbm_display;

	TPL_ASSERT(tpl_display);
	TPL_ASSERT(native_dpy);

	tpl_gbm_display = (tpl_gbm_display_t *) tpl_display->backend.data;
	tpl_display->bufmgr_fd = dup(gbm_device_get_fd(tpl_display->native_handle));
	tpl_gbm_display->bufmgr = tbm_bufmgr_init(tpl_display->bufmgr_fd);
	if (tpl_gbm_display->bufmgr == NULL)
	{
		TPL_ERR("TBM buffer manager initialization failed!");
		return TPL_FALSE;
	}

	tpl_gbm_display->bound_client_display = TPL_TRUE;
	return TPL_TRUE;
}

unsigned int __tpl_gbm_display_unbind_client_wayland_display(tpl_display_t *tpl_display, tpl_handle_t native_dpy)
{
	tpl_gbm_display_t *tpl_gbm_display;

	TPL_ASSERT(tpl_display);

	tpl_gbm_display = (tpl_gbm_display_t *) tpl_display->backend.data;

	tbm_bufmgr_deinit(tpl_gbm_display->bufmgr);
	close(tpl_display->bufmgr_fd);
	tpl_gbm_display->bound_client_display = TPL_FALSE;
	return TPL_TRUE;
}
#endif
