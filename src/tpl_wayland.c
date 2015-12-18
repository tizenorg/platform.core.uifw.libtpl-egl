#define inline __inline__

#include <wayland-client.h>

#include "wayland-egl/wayland-egl-priv.h"

#include <drm.h>
#include <tbm_bufmgr.h>
#include <gbm.h>
#ifndef USE_TBM_QUEUE
#define USE_TBM_QUEUE
#endif
#include <gbm/gbm_tbm.h>
#include <gbm/gbm_tbmint.h>
#include <xf86drm.h>

#undef inline

#include "tpl_internal.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tbm_surface_queue.h>
#include <wayland-tbm-client.h>
#include <wayland-tbm-server.h>

#define CLIENT_QUEUE_SIZE 3

/* In wayland, application and compositor create its own drawing buffers. Recommend size is more than 2. */
#define TPL_BUFFER_ALLOC_SIZE_APP               3
#define TPL_BUFFER_ALLOC_SIZE_COMPOSITOR        4
#define TPL_BUFFER_ALLOC_SIZE_MAX		(((TPL_BUFFER_ALLOC_SIZE_APP) > (TPL_BUFFER_ALLOC_SIZE_COMPOSITOR))?(TPL_BUFFER_ALLOC_SIZE_APP):(TPL_BUFFER_ALLOC_SIZE_COMPOSITOR))

typedef struct _tpl_wayland_display       tpl_wayland_display_t;
typedef struct _tpl_wayland_surface       tpl_wayland_surface_t;
typedef struct _tpl_wayland_buffer        tpl_wayland_buffer_t;

enum wayland_display_type
{
	SERVER,
	CLIENT
};

enum wayland_buffer_status
{
	IDLE = 0,
	BUSY = 1,
	READY = 2, /* redering done */
	POSTED = 3 /* gbm locked */
};

struct _tpl_wayland_display
{
	tbm_bufmgr         bufmgr;
	struct wayland_tbm_client	*wl_tbm_client;
	enum wayland_display_type	type;
	union
	{
		struct
		{
			tpl_bool_t               authenticated;
			struct wl_event_queue   *wl_queue;
			struct wl_registry      *wl_registry;
			/* deprecated */
			tpl_list_t		cached_buffers;
		} app;
		struct
		{
			tpl_list_t              cached_buffers;
			tpl_bool_t              bound_client_display;
		} comp;
	} proc;
};

struct _tpl_wayland_surface
{
	tpl_buffer_t	*current_rendering_buffer;
	tpl_list_t		done_rendering_queue;
	int				current_back_idx;
	tbm_surface_queue_h tbm_queue;
};

struct _tpl_wayland_buffer
{
	tpl_display_t	*display;
	tbm_surface_h	tbm_surface;
	tbm_bo			bo;
	int				reused;
	tpl_buffer_t *tpl_buffer;

	enum wayland_buffer_status status;
	union
	{
		struct
		{
			struct wl_proxy		*wl_proxy;
			tpl_bool_t			resized;
		} app;
		struct
		{
			struct gbm_bo		*gbm_bo;
			tpl_bool_t			posted;
			struct wl_listener	destroy_listener;
		} comp;
	} proc;
};

static const struct wl_registry_listener registry_listener;
static const struct wl_callback_listener sync_listener;
static const struct wl_callback_listener frame_listener;
static const struct wl_buffer_listener buffer_release_listener;

#ifdef EGL_BIND_WL_DISPLAY
unsigned int __tpl_wayland_display_bind_client_display(tpl_display_t  *tpl_display,  tpl_handle_t native_dpy);
unsigned int __tpl_wayland_display_unbind_client_display(tpl_display_t  *tpl_display, tpl_handle_t native_dpy);
#endif

#define TPL_BUFFER_CACHE_MAX_ENTRIES 40

static TPL_INLINE tpl_bool_t
__tpl_wayland_surface_buffer_cache_add(tpl_list_t *buffer_cache, tpl_buffer_t *buffer)
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

static void
__tpl_wayland_surface_buffer_free(tpl_buffer_t *buffer);

static TPL_INLINE void
__tpl_wayland_surface_buffer_cache_remove_all(tpl_list_t *buffer_cache)
{
	tpl_list_node_t *node;

	TPL_ASSERT(buffer_cache);

	node = __tpl_list_get_front_node(buffer_cache);

	while (node)
	{
		tpl_buffer_t *buffer = (tpl_buffer_t *)__tpl_list_node_get_data(node);

		TPL_ASSERT(buffer);

		tpl_object_unreference(&buffer->base);

		__tpl_list_remove(node, __tpl_wayland_surface_buffer_free);
		node = __tpl_list_node_next(node);
	}
}

static TPL_INLINE void
__tpl_wayland_surface_buffer_cache_remove(tpl_list_t *buffer_cache, size_t name)
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
__tpl_wayland_surface_buffer_cache_find(tpl_list_t *buffer_cache, size_t name)
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
__tpl_wayland_display_is_wl_display(tpl_handle_t native_dpy)
{
	TPL_ASSERT(native_dpy);

	if (*(void **)native_dpy == gbm_create_device)
		return TPL_FALSE;

	{
		struct wl_interface *wl_egl_native_dpy = *(void **) native_dpy;

	/* MAGIC CHECK: A native display handle is a wl_display if the de-referenced first value
	   is a memory address pointing the structure of wl_display_interface. */
		if ( wl_egl_native_dpy == &wl_display_interface )
		{
			return TPL_TRUE;
		}

		if(strncmp(wl_egl_native_dpy->name, wl_display_interface.name, strlen(wl_display_interface.name)) == 0)
		{
			return TPL_TRUE;
		}
	}

	return TPL_FALSE;
}

static TPL_INLINE tpl_bool_t
__tpl_wayland_display_is_gbm_device(tpl_handle_t native_dpy)
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

static int
__tpl_wayland_display_roundtrip(tpl_display_t *display)
{
	struct wl_display *wl_dpy;
	tpl_wayland_display_t *wayland_display;
	struct wl_callback *callback;
	int done = 0, ret = 0;

	TPL_ASSERT(display);
	TPL_ASSERT(display->native_handle);
	TPL_ASSERT(display->backend.data);

	wl_dpy = (struct wl_display *) display->native_handle;
	wayland_display = (tpl_wayland_display_t *) display->backend.data;

	callback = wl_display_sync(wl_dpy);
	wl_callback_add_listener(callback, &sync_listener, &done);

	wl_proxy_set_queue((struct wl_proxy *) callback, wayland_display->proc.app.wl_queue);

	while (ret != -1 && !done)
	{
		ret = wl_display_dispatch_queue(wl_dpy, wayland_display->proc.app.wl_queue);
	}

	return ret;
}

static tpl_bool_t
__tpl_wayland_display_init(tpl_display_t *display)
{
	tpl_wayland_display_t *wayland_display = NULL;

	TPL_ASSERT(display);

	/* Do not allow default display in wayland. */
	if (display->native_handle == NULL)
		return TPL_FALSE;

	wayland_display = (tpl_wayland_display_t *) calloc(1, sizeof(tpl_wayland_display_t));
	if (wayland_display == NULL)
		return TPL_FALSE;

	display->backend.data = wayland_display;
	display->bufmgr_fd = -1;

	if (__tpl_wayland_display_is_wl_display(display->native_handle))
	{
		struct wl_display *wl_dpy = (struct wl_display *)display->native_handle;

		wayland_display->type = CLIENT;
		wayland_display->wl_tbm_client = wayland_tbm_client_init((struct wl_display *) wl_dpy);

		if (wayland_display->wl_tbm_client == NULL)
		{
			TPL_ERR("Wayland TBM initialization failed!");
			goto free_wl_display;
		}

		wayland_display->proc.app.wl_queue = wl_display_create_queue(wl_dpy);
		if (NULL == wayland_display->proc.app.wl_queue)
			goto free_wl_display;

		wayland_display->proc.app.wl_registry = wl_display_get_registry(wl_dpy);
		if (NULL == wayland_display->proc.app.wl_registry)
			goto destroy_queue;

		wl_proxy_set_queue((struct wl_proxy *)wayland_display->proc.app.wl_registry, wayland_display->proc.app.wl_queue);
		__tpl_list_init(&wayland_display->proc.app.cached_buffers);
	}
	else if (__tpl_wayland_display_is_gbm_device(display->native_handle))
	{
		wayland_display->type = SERVER;

		__tpl_list_init(&wayland_display->proc.comp.cached_buffers);
	}
	else
		goto free_wl_display;

	return TPL_TRUE;
destroy_queue:
	wl_event_queue_destroy(wayland_display->proc.app.wl_queue);
free_wl_display:
	if (wayland_display != NULL)
	{
		free(wayland_display);
		display->backend.data = NULL;
	}
	return TPL_FALSE;
}

static void
__tpl_wayland_display_fini(tpl_display_t *display)
{
	tpl_wayland_display_t *wayland_display;

	TPL_ASSERT(display);

	wayland_display = (tpl_wayland_display_t *)display->backend.data;
	if (wayland_display != NULL)
	{
		if (wayland_display->type == CLIENT)
		{
			wayland_tbm_client_deinit(wayland_display->wl_tbm_client);
			__tpl_list_fini(&wayland_display->proc.app.cached_buffers, (tpl_free_func_t) tpl_object_unreference);
		}
		if (wayland_display->type == SERVER)
		{
			if (wayland_display->proc.comp.bound_client_display)
				__tpl_wayland_display_unbind_client_display(display, NULL);

			__tpl_list_fini(&wayland_display->proc.comp.cached_buffers, (tpl_free_func_t) tpl_object_unreference);
		}

		free(wayland_display);
	}
	display->backend.data = NULL;
}

static tpl_bool_t
__tpl_wayland_display_query_config(tpl_display_t *display, tpl_surface_type_t surface_type,
				   int red_size, int green_size, int blue_size, int alpha_size,
				   int color_depth, int *native_visual_id, tpl_bool_t *is_slow)
{
	TPL_ASSERT(display);

	tpl_wayland_display_t* wayland_display = (tpl_wayland_display_t*)display->backend.data;

	if (surface_type == TPL_SURFACE_TYPE_WINDOW &&
		red_size == 8 &&
		green_size == 8 &&
		blue_size == 8 &&
		(color_depth == 32 || color_depth == 24))
	{
		if (alpha_size == 8)
		{
			if (wayland_display->type == CLIENT)
			{
				if (native_visual_id != NULL) *native_visual_id = TBM_FORMAT_ARGB8888;
			}
			else if (wayland_display->type == SERVER &&
				 gbm_device_is_format_supported((struct gbm_device *)display->native_handle,
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
			if (wayland_display->type == CLIENT)
			{
				if (native_visual_id != NULL) *native_visual_id = TBM_FORMAT_XRGB8888;
			}
			else if (wayland_display->type == SERVER &&
				 gbm_device_is_format_supported((struct gbm_device *)display->native_handle,
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
__tpl_wayland_display_filter_config(tpl_display_t *display,
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
__tpl_wayland_display_get_window_info(tpl_display_t *display, tpl_handle_t window,
				      int *width, int *height, tpl_format_t *format, int depth, int a_size)
{
	TPL_ASSERT(display);
	TPL_ASSERT(window);

	tpl_wayland_display_t* wayland_display = (tpl_wayland_display_t*)display->backend.data;

	if (wayland_display->type == CLIENT)
	{
		struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)window;

		if (format != NULL)
		{
			/* Wayland-egl window doesn't have native format information.
			   It is fixed from 'EGLconfig' when called eglCreateWindowSurface().
			   So we use the tpl_surface format instead. */
			tpl_surface_t *surface = wl_egl_window->private;
			if (surface != NULL)
				*format = surface->format;
			else
			{
				if (a_size == 8)
					*format = TPL_FORMAT_ARGB8888;
				else if (a_size == 0)
					*format = TPL_FORMAT_XRGB8888;
			}
		}
		if (width != NULL) *width = wl_egl_window->width;
		if (height != NULL) *height = wl_egl_window->height;

		return TPL_TRUE;
	}
	else if (wayland_display->type == SERVER)
	{
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
	}

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_wayland_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
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
__tpl_wayland_display_flush(tpl_display_t *display)
{
	TPL_IGNORE(display);

	/* Do nothing. */
}

static tpl_bool_t
__tpl_wayland_surface_init(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_wayland_display_t *wayland_display = NULL;

	TPL_ASSERT(surface);

	wayland_surface = (tpl_wayland_surface_t *) calloc(1, sizeof(tpl_wayland_surface_t));
	if (NULL == wayland_surface)
		return TPL_FALSE;

	surface->backend.data = (void *)wayland_surface;
	wayland_surface->current_back_idx = 0;
	wayland_surface->tbm_queue = NULL;

	__tpl_list_init(&wayland_surface->done_rendering_queue);

	wayland_display = (tpl_wayland_display_t*)surface->display->backend.data;

	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		if (wayland_display->type == CLIENT)
		{
                        int tbm_format;
			struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)surface->native_handle;
			wl_egl_window->private = surface;

			switch (surface->format)
			{
				case TPL_FORMAT_ARGB8888:
					tbm_format = TBM_FORMAT_ARGB8888;
					break;
				case TPL_FORMAT_XRGB8888:
					tbm_format = TBM_FORMAT_XRGB8888;
					break;
				case TPL_FORMAT_RGB565:
					tbm_format = TBM_FORMAT_RGB565;
					break;
				default:
					TPL_ERR("Unsupported format found in surface!");
					return TPL_FALSE;
			}

			wayland_surface->tbm_queue = tbm_surface_queue_create(
					CLIENT_QUEUE_SIZE,
					wl_egl_window->width,
                                        wl_egl_window->height,
					tbm_format,
                                        0);
		}
		if (wayland_display->type == SERVER)
		{
			struct gbm_surface *gbm_surface = (struct gbm_surface*)surface->native_handle;
			wayland_surface->tbm_queue = (tbm_surface_queue_h)gbm_tbm_get_surface_queue(gbm_surface);
		}

		if (TPL_TRUE != __tpl_wayland_display_get_window_info(surface->display, surface->native_handle,
					&surface->width, &surface->height, NULL, 0, 0))
			goto error;

		TPL_LOG(3, "window(%p, %p) %dx%d", surface, surface->native_handle, surface->width, surface->height);
		return TPL_TRUE;
	}
	else if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		if (TPL_TRUE != __tpl_wayland_display_get_pixmap_info(surface->display, surface->native_handle,
					&surface->width, &surface->height, NULL))
			goto error;

		return TPL_TRUE;
	}

error:
	free(wayland_surface);

	return TPL_FALSE;
}

static void
__tpl_wayland_surface_buffer_free(tpl_buffer_t *buffer)
{
	TPL_LOG(3, "buffer(%p) key:%zu", buffer, buffer?buffer->key:-1);
	if (buffer != NULL)
	{
		tpl_object_unreference((tpl_object_t *) buffer);
	}
}

static tpl_bool_t
__tpl_wayland_surface_destroy_cached_buffers(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_wayland_display_t *wayland_display = NULL;

	if (surface == NULL)
	{
		TPL_ERR("tpl surface is invalid!!\n");
		return TPL_FALSE;
	}

	wayland_surface = (tpl_wayland_surface_t*)surface->backend.data;
	wayland_display = (tpl_wayland_display_t*)surface->display->backend.data;

	if (wayland_surface == NULL || wayland_display == NULL)
	{
		TPL_ERR("tpl surface has invalid members!!\n");
		return TPL_FALSE;
	}

	if (wayland_display->type == CLIENT)
		__tpl_wayland_surface_buffer_cache_remove_all(&wayland_display->proc.app.cached_buffers);


	return TPL_TRUE;
}

static tpl_bool_t
__tpl_wayland_surface_update_cached_buffers(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_wayland_display_t *wayland_display = NULL;

	if (surface == NULL)
	{
		TPL_ERR("tpl surface is invalid!!\n");
		return TPL_FALSE;
	}

	wayland_surface = (tpl_wayland_surface_t*)surface->backend.data;
	wayland_display = (tpl_wayland_display_t*)surface->display->backend.data;

	if (wayland_surface == NULL || wayland_display == NULL)
	{
		TPL_ERR("tpl surface has invalid members!!\n");
		return TPL_FALSE;
	}

	return TPL_TRUE;
}

static void
__tpl_wayland_surface_fini(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_wayland_display_t *wayland_display = NULL;

	TPL_ASSERT(surface);

	wayland_surface = (tpl_wayland_surface_t *) surface->backend.data;
	if (NULL == wayland_surface)
		return;

	wayland_display = (tpl_wayland_display_t *) surface->display->backend.data;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	__tpl_wayland_surface_buffer_cache_remove_all(&wayland_display->proc.app.cached_buffers);

	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		if (wayland_display->type == CLIENT)
		{
			struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)surface->native_handle;

			TPL_ASSERT(wl_egl_window);
			/* TPL_ASSERT(wl_egl_window->surface); */ /* to be enabled once evas/gl patch is in place */

			wl_egl_window->private = NULL;

			/* Detach all pending buffers */
			if (wl_egl_window->surface && /* if-statement to be removed once evas/gl patch is in place */
				wl_egl_window->width == wl_egl_window->attached_width &&
				wl_egl_window->height == wl_egl_window->attached_height)
			{
				wl_surface_attach(wl_egl_window->surface, NULL, 0, 0);
				wl_surface_commit(wl_egl_window->surface);
			}

			wl_display_flush(surface->display->native_handle);
			__tpl_wayland_display_roundtrip(surface->display);

			tbm_surface_queue_destroy(wayland_surface->tbm_queue);
			wayland_surface->tbm_queue = NULL;
		}
	}

	free(wayland_surface);
	surface->backend.data = NULL;
}

static void
__tpl_wayland_surface_post(tpl_surface_t *surface, tpl_frame_t *frame)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);
	TPL_ASSERT(frame);
	TPL_ASSERT(frame->buffer);

	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t*) surface->display->backend.data;
	tpl_wayland_buffer_t *wayland_buffer = NULL;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (wayland_display->type == CLIENT)
	{
		wayland_buffer = (tpl_wayland_buffer_t *)frame->buffer->backend.data;
		struct wl_egl_window *wl_egl_window = NULL;
		int i;
		tbm_bo_handle bo_handle = tbm_bo_get_handle(wayland_buffer->bo , TBM_DEVICE_CPU);
                if (bo_handle.ptr != NULL)
                        TPL_IMAGE_DUMP(bo_handle.ptr, surface->width, surface->height, surface->dump_count++);
		TPL_LOG(3, "\t buffer(%p, %p) key:%zu", frame->buffer, wayland_buffer->proc.app.wl_proxy, frame->buffer->key);

		wl_egl_window = (struct wl_egl_window *)surface->native_handle;

		tpl_object_reference((tpl_object_t *)frame->buffer);
		wl_surface_attach(wl_egl_window->surface,
			(void *)wayland_buffer->proc.app.wl_proxy,
			wl_egl_window->dx,
			wl_egl_window->dy);

		wl_egl_window->attached_width = wl_egl_window->width;
		wl_egl_window->attached_height = wl_egl_window->height;

		for (i = 0; i < frame->damage.num_rects; i++)
		{
			wl_surface_damage(wl_egl_window->surface,
				frame->damage.rects[i * 4 + 0],
				frame->damage.rects[i * 4 + 1],
				frame->damage.rects[i * 4 + 2],
				frame->damage.rects[i * 4 + 3]);
		}
		if (frame->damage.num_rects == 0) {
			wl_surface_damage(wl_egl_window->surface,
					  wl_egl_window->dx, wl_egl_window->dy,
					  wl_egl_window->width, wl_egl_window->height);
		}

		{
			/* Register a meaningless surface frame callback.
			   Because the buffer_release callback only be triggered if this callback is registered. */
			struct wl_callback *frame_callback = NULL;
			frame_callback = wl_surface_frame(wl_egl_window->surface);
			wl_callback_add_listener(frame_callback, &frame_listener, frame->buffer);
			wl_proxy_set_queue((struct wl_proxy *)frame_callback, wayland_display->proc.app.wl_queue);
		}
		wl_surface_commit(wl_egl_window->surface);

		wl_display_flush(surface->display->native_handle);

		wayland_buffer->status = POSTED;

		TPL_LOG(7, "BO:%d", tbm_bo_export(wayland_buffer->bo));
	}
	else if (wayland_display->type == SERVER)
	{
		tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t*)surface->backend.data;
		tpl_buffer_t *buffer = NULL;

		if (!__tpl_list_is_empty(&wayland_surface->done_rendering_queue))
		{
			buffer = __tpl_list_pop_front(&wayland_surface->done_rendering_queue, NULL);

			TPL_ASSERT(buffer);

			wayland_buffer = (tpl_wayland_buffer_t *) buffer->backend.data;
		}

		tbm_surface_internal_unref(wayland_buffer->tbm_surface);

		if (wayland_surface->tbm_queue && wayland_buffer->tbm_surface)
		{
			tbm_surface_queue_enqueue(wayland_surface->tbm_queue, wayland_buffer->tbm_surface);
			TPL_LOG(6, "tbm_surface ENQUEUED!!");
		}
	}
}

static tpl_bool_t
__tpl_wayland_surface_begin_frame(tpl_surface_t *surface)
{
	tpl_wayland_display_t *wayland_display;
	tpl_wayland_surface_t *wayland_surface;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	wayland_display = (tpl_wayland_display_t *) surface->display->backend.data;
	wayland_surface = (tpl_wayland_surface_t *) surface->backend.data;

	TPL_ASSERT(wayland_surface->current_rendering_buffer == NULL);

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (wayland_display->type == CLIENT)
	{
		TPL_OBJECT_UNLOCK(surface);

		__tpl_wayland_display_roundtrip(surface->display);

		TPL_OBJECT_LOCK(surface);
	}

	if (wayland_display->type == CLIENT)
		wayland_surface->current_rendering_buffer = NULL;

	if ( wayland_display->type == CLIENT && wayland_surface->current_rendering_buffer )
	{
		tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t*)wayland_surface->current_rendering_buffer->backend.data;
		wayland_buffer->status = BUSY;
		TPL_LOG(6, "current_rendering_buffer BO:%d", tbm_bo_export(wayland_buffer->bo));
	}
	return TPL_TRUE;
}

static tpl_bool_t
__tpl_wayland_surface_validate_frame(tpl_surface_t *surface)
{
	TPL_IGNORE(surface);

	return TPL_TRUE;
}

static tpl_bool_t
__tpl_wayland_surface_end_frame(tpl_surface_t *surface)
{
	tpl_wayland_surface_t	*wayland_surface = NULL;
	tpl_wayland_buffer_t	*wayland_buffer	 = NULL;
	tpl_wayland_display_t	*wayland_display = NULL;
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	wayland_surface = (tpl_wayland_surface_t *) surface->backend.data;
	wayland_display = (tpl_wayland_display_t *) surface->display->backend.data;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (wayland_surface->current_rendering_buffer != NULL)
	{
		wayland_buffer = (tpl_wayland_buffer_t *) wayland_surface->current_rendering_buffer->backend.data;

		wayland_buffer->status = READY;

		TPL_LOG(6, "current_rendering_buffer BO:%d", tbm_bo_export(wayland_buffer->bo));
	}

	/* MOVE BUFFER : (current buffer) --> [done queue] */
	if (wayland_display->type == SERVER && TPL_TRUE != __tpl_list_push_back(&wayland_surface->done_rendering_queue, wayland_surface->current_rendering_buffer))
		return TPL_FALSE;

	wayland_surface->current_rendering_buffer = NULL;

	return TPL_TRUE;
}

static int
__tpl_wayland_get_depth_from_format(tpl_format_t format)
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

static tpl_buffer_t *
__tpl_wayland_surface_create_buffer_from_wl_egl(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tpl_wayland_surface_t *wayland_surface = NULL;
	tbm_bo bo;
	tbm_bo_handle bo_handle;
	int width, height, depth;
	uint32_t stride, size, offset;
	uint32_t key;
	tpl_format_t tpl_format;

	tpl_wayland_display_t *wayland_display;

	tbm_surface_queue_error_e tsq_err = 0;
	int format;
	tbm_surface_h           tbm_surface = NULL;
/* TODO: If HW support getting of  gem memory size,
		use tbm_surface_get_info() with tbm_surface_info_s  */
#if 0
	tbm_surface_info_s	tbm_surf_info;
#endif
	struct wl_proxy		*wl_proxy = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->native_handle);
	TPL_ASSERT(surface->display->backend.data);

	wayland_display = (tpl_wayland_display_t *) surface->display->backend.data;
	wayland_surface = (tpl_wayland_surface_t *) surface->backend.data;


	struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)surface->native_handle;
	width = wl_egl_window->width;
	height = wl_egl_window->height;
	format = tbm_surface_queue_get_format(wayland_surface->tbm_queue);

	/* Check whether the surface was resized by wayland_egl */
	if (width != tbm_surface_queue_get_width(wayland_surface->tbm_queue) ||
        height != tbm_surface_queue_get_height(wayland_surface->tbm_queue))
	{
		tbm_surface_queue_reset(wayland_surface->tbm_queue, width, height, format);

		/* Remove all cached buffers (This code will be deleted.)*/
		__tpl_wayland_surface_buffer_cache_remove_all(&wayland_display->proc.app.cached_buffers);
		if (reset_buffers != NULL)
			*reset_buffers = TPL_TRUE;
	}

	tsq_err = tbm_surface_queue_dequeue(wayland_surface->tbm_queue, &tbm_surface);
	if (tbm_surface == NULL)
	{
		TPL_LOG(6, "Wait until dequeable | tsq_err = %d", tsq_err);
		tbm_surface_queue_can_dequeue(wayland_surface->tbm_queue, 1);

		tsq_err = tbm_surface_queue_dequeue(wayland_surface->tbm_queue, &tbm_surface);
		if (tbm_surface == NULL)
		{
			TPL_ERR("Failed to get tbm_surface from tbm_surface_queue | tsq_err = %d",tsq_err);
			return NULL;
		}
	}

	tbm_surface_internal_ref(tbm_surface);

	/* Get tbm_bo from tbm_surface_h */
	bo = tbm_surface_internal_get_bo(tbm_surface, 0);
	if (NULL == bo)
	{
		TPL_ERR("TBM get bo failed!");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	key = tbm_bo_export(bo);

	buffer = __tpl_wayland_surface_buffer_cache_find(&wayland_display->proc.app.cached_buffers, key);
	if (buffer != NULL)
	{
		return buffer;
	}

	width = tbm_surface_get_width(tbm_surface);
	height = tbm_surface_get_height(tbm_surface);

	switch(tbm_surface_get_format(tbm_surface))
	{
		case TBM_FORMAT_ARGB8888: tpl_format = TPL_FORMAT_ARGB8888; break;
		case TBM_FORMAT_XRGB8888: tpl_format = TPL_FORMAT_XRGB8888; break;
		case TBM_FORMAT_RGB565: tpl_format = TPL_FORMAT_RGB565; break;
		default:
		tpl_format = TPL_FORMAT_INVALID;
		TPL_ERR("No matched format!!");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	depth = __tpl_wayland_get_depth_from_format(tpl_format);

	/* Get pitch stride from tbm_surface */
	tbm_surface_internal_get_plane_data(tbm_surface, 0, &size, &offset, &stride);

	/* Create tpl buffer. */
	bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);
	if (bo_handle.ptr == NULL)
	{
		TPL_ERR("TBM bo get handle failed!");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	buffer = __tpl_buffer_alloc(surface, (size_t) key,
	                  (int)bo_handle.u32, width, height, depth, stride);

        if (buffer == NULL)
	{
		TPL_ERR("Failed to allocate tpl buffer | surf:%p bo_hnd:%d WxHxD:%dx%dx%d",
			surface, (int) bo_handle.u32, width, height, depth);
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	wayland_buffer = (tpl_wayland_buffer_t *) calloc(1, sizeof(tpl_wayland_buffer_t));
	if (wayland_buffer == NULL)
	{
		TPL_ERR("Mem alloc for wayland_buffer failed!");
		tpl_object_unreference((tpl_object_t *) buffer);
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	buffer->backend.data = (void *)wayland_buffer;

	if (TPL_TRUE != __tpl_wayland_surface_buffer_cache_add(&wayland_display->proc.app.cached_buffers, buffer))
	{
		TPL_ERR("Adding surface to buffer cache failed!");
		tpl_object_unreference((tpl_object_t *) buffer);
		tbm_surface_internal_unref(tbm_surface);
		free(wayland_buffer);
		return NULL;
	}

	wl_proxy = (struct wl_proxy *)wayland_tbm_client_create_buffer(wayland_display->wl_tbm_client,
			tbm_surface);

	if (wl_proxy == NULL)
	{
		TPL_ERR("Failed to create TBM client buffer!");
		tbm_surface_internal_unref(tbm_surface);
		tbm_surface_destroy(tbm_surface);
		tpl_object_unreference((tpl_object_t *)buffer);
		free(wayland_buffer);
		return NULL;
	}

	wl_proxy_set_queue(wl_proxy, wayland_display->proc.app.wl_queue);
	wl_buffer_add_listener((void *)wl_proxy, &buffer_release_listener, buffer);

	wl_display_flush((struct wl_display *)surface->display->native_handle);

	wayland_buffer->display = surface->display;
	wayland_buffer->tbm_surface = tbm_surface;
	wayland_buffer->proc.app.wl_proxy = wl_proxy;
	wayland_buffer->bo = bo;

	wayland_surface = (tpl_wayland_surface_t*) surface->backend.data;

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "buffer:%p bo_hnd:%d, %dx%d", buffer, (int) bo_handle.u32, width, height);

	return buffer;
}

static int tpl_buffer_key;
#define KEY_TPL_BUFFER  (unsigned long)(&tpl_buffer_key)

static inline tpl_buffer_t *
__tpl_wayland_surface_get_buffer_from_tbm_surface(tbm_surface_h surface)
{
    tbm_bo bo;
    tpl_buffer_t* buf=NULL;

    bo = tbm_surface_internal_get_bo(surface, 0);
    tbm_bo_get_user_data(bo, KEY_TPL_BUFFER, (void **)&buf);

    return buf;
}

#if 0
static inline void
__tpl_wayland_buffer_set_tbm_surface(tbm_surface_h surface, tpl_buffer_t *buf)
{
    tbm_bo bo;

    bo = tbm_surface_internal_get_bo(surface, 0);
    tbm_bo_add_user_data(bo, KEY_TPL_BUFFER, NULL);
    tbm_bo_set_user_data(bo, KEY_TPL_BUFFER, buf);
}
#endif

static tpl_buffer_t *
__tpl_wayland_surface_create_buffer_from_gbm_surface(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tbm_bo bo;
	tbm_surface_h tbm_surface = NULL;
	tbm_surface_queue_error_e tsq_err = 0;

	tbm_bo_handle bo_handle;
	int width, height, depth;
	uint32_t size, offset, stride, key;
	tpl_format_t format;
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_wayland_display_t *wayland_display = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->native_handle);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);

	wayland_surface = (tpl_wayland_surface_t*)surface->backend.data;
	wayland_display = (tpl_wayland_display_t*)surface->display->backend.data;

	tsq_err = tbm_surface_queue_dequeue(wayland_surface->tbm_queue, &tbm_surface);
	if (tbm_surface == NULL)
	{
		TPL_LOG(6, "Wait until dequeable | tsq_err = %d", tsq_err);
		tbm_surface_queue_can_dequeue(wayland_surface->tbm_queue, 1);

		tsq_err = tbm_surface_queue_dequeue(wayland_surface->tbm_queue, &tbm_surface);
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

	buffer = __tpl_wayland_surface_buffer_cache_find(&wayland_display->proc.comp.cached_buffers, key);
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

	depth = __tpl_wayland_get_depth_from_format(format);

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

	wayland_buffer = (tpl_wayland_buffer_t *) calloc(1, sizeof(tpl_wayland_buffer_t));
	if (wayland_buffer == NULL)
	{
		TPL_ERR("Mem alloc for wayland_buffer failed!");
		tpl_object_unreference((tpl_object_t *) buffer);
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	buffer->backend.data = (void *)wayland_buffer;

	/* Post process */
	wayland_buffer->display = surface->display;
	wayland_buffer->bo = bo;
	wayland_buffer->tbm_surface = tbm_surface;

	if (TPL_TRUE != __tpl_wayland_surface_buffer_cache_add(&wayland_display->proc.comp.cached_buffers, buffer))
	{
		TPL_ERR("Adding surface to buffer cache failed!");
		tpl_object_unreference((tpl_object_t *) buffer);
		tbm_surface_internal_unref(tbm_surface);
		free(wayland_buffer);
		return NULL;
	}

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "buffer:%p bo_hnd:%d, %dx%d", buffer, (int) bo_handle.u32, width, height);

	return buffer;
}

static void
__tpl_wayland_buffer_destroy_notify(struct wl_listener *listener, void *data)
{
	tpl_display_t *display;
	tpl_wayland_display_t *wayland_display;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	size_t key = 0;

	wayland_buffer = wl_container_of(listener, wayland_buffer, proc.comp.destroy_listener);
	display = wayland_buffer->display;
	key = tbm_bo_export(wayland_buffer->bo);
	wayland_display = (tpl_wayland_display_t *)display->backend.data;
	tpl_object_unreference((tpl_object_t *)wayland_buffer->tpl_buffer);
	__tpl_wayland_surface_buffer_cache_remove(&wayland_display->proc.comp.cached_buffers, key);
}

static tpl_buffer_t *
__tpl_wayland_surface_create_buffer_from_wl_tbm(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
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

	tpl_wayland_display_t *wayland_display;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->native_handle);

	wayland_display = (tpl_wayland_display_t *) surface->display->backend.data;

	tbm_surface = wayland_tbm_server_get_surface(NULL, (struct wl_resource*)surface->native_handle);
	if (tbm_surface == NULL)
	{
		TPL_ERR("Failed to get tbm surface!");
		return NULL;
	}

	bo = tbm_surface_internal_get_bo(tbm_surface, 0);
	key = tbm_bo_export(bo);

	buffer = __tpl_wayland_surface_buffer_cache_find(&wayland_display->proc.comp.cached_buffers, key);
	if (buffer != NULL)
	{
		__tpl_buffer_set_surface(buffer, surface);
	}
	else
	{
		/* Inc ref count about tbm_surface */
		/* It will be dec when wayland_buffer_fini called*/
		tbm_surface_internal_ref(tbm_surface);

		if (TPL_TRUE != __tpl_wayland_display_get_pixmap_info(
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
		depth = __tpl_wayland_get_depth_from_format(format);

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

		wayland_buffer = (tpl_wayland_buffer_t *) calloc(1, sizeof(tpl_wayland_buffer_t));
		if (wayland_buffer == NULL)
		{
			TPL_ERR("Mem alloc failed for wayland buffer!");
			tpl_object_unreference((tpl_object_t *) buffer);
			tbm_surface_internal_unref(tbm_surface);
			return NULL;
		}

		wayland_buffer->display = surface->display;
		wayland_buffer->bo = bo;
		wayland_buffer->tbm_surface = tbm_surface;
		wayland_buffer->tpl_buffer = buffer;

		buffer->backend.data = (void *)wayland_buffer;
		buffer->key = key;

		if (TPL_TRUE != __tpl_wayland_surface_buffer_cache_add(&wayland_display->proc.comp.cached_buffers, buffer))
		{
			TPL_ERR("Adding surface to buffer cache failed!");
			tpl_object_unreference((tpl_object_t *) buffer);
			tbm_surface_internal_unref(tbm_surface);
			free(wayland_buffer);
			return NULL;
		}

		wayland_buffer->proc.comp.destroy_listener.notify = __tpl_wayland_buffer_destroy_notify;
		wl_resource_add_destroy_listener((struct wl_resource*)surface->native_handle, &wayland_buffer->proc.comp.destroy_listener);
	}

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	return buffer;
}

static tpl_buffer_t *
__tpl_wayland_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	int width, height;
	tpl_wayland_surface_t *wayland_surface;
	tpl_wayland_display_t *wayland_display;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->backend.data);

	wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;
	wayland_display = (tpl_wayland_display_t *)surface->display->backend.data;

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "window(%p, %p), current(%p)", surface, surface->native_handle,
		wayland_surface->current_rendering_buffer);

	if (surface->type != TPL_SURFACE_TYPE_PIXMAP &&
		TPL_TRUE != __tpl_wayland_display_get_window_info(surface->display,
		surface->native_handle, &width, &height, NULL, 0, 0))
	{
		TPL_ERR("Failed to get window info!");
		return NULL;
	}

	if (wayland_surface->current_rendering_buffer == NULL)
	{
		if (surface->type == TPL_SURFACE_TYPE_WINDOW)
		{
			if (wayland_display->type == CLIENT)
			{
				wayland_surface->current_rendering_buffer =
					__tpl_wayland_surface_create_buffer_from_wl_egl(surface, reset_buffers);
			}
			if (wayland_display->type == SERVER)
			{
				wayland_surface->current_rendering_buffer =
					__tpl_wayland_surface_create_buffer_from_gbm_surface(surface, reset_buffers);
			}
		}
		if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
		{
                        wayland_surface->current_rendering_buffer =
				__tpl_wayland_surface_create_buffer_from_wl_tbm(surface, reset_buffers);
		}
		TPL_LOG(3, "window(%p, %p), current(%p)", surface, surface->native_handle,
				wayland_surface->current_rendering_buffer);
	}

        TPL_ASSERT(wayland_surface->current_rendering_buffer);

	return wayland_surface->current_rendering_buffer;
}

static tpl_bool_t
__tpl_wayland_buffer_init(tpl_buffer_t *buffer)
{
	TPL_IGNORE(buffer);

	return TPL_TRUE;
}

static void
__tpl_wayland_buffer_fini(tpl_buffer_t *buffer)
{
	TPL_ASSERT(buffer);

	TPL_LOG(3, "tpl_buffer(%p) key:%zu fd:%d %dx%d", buffer, buffer->key, buffer->fd, buffer->width, buffer->height);

	if (buffer->backend.data)
	{
		tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;

		tpl_wayland_display_t *wayland_display =
			(tpl_wayland_display_t *)wayland_buffer->display->backend.data;

		if (wayland_buffer->bo != NULL && wayland_buffer->tbm_surface != NULL)
		{
			tbm_surface_internal_unref(wayland_buffer->tbm_surface);
			tbm_surface_destroy(wayland_buffer->tbm_surface);
			wayland_buffer->bo = NULL;
			wayland_buffer->tbm_surface = NULL;
		}

		if (wayland_display->type == CLIENT)
		{
			wl_display_flush((struct wl_display *)wayland_buffer->display->native_handle);

			if (wayland_buffer->proc.app.wl_proxy != NULL)
				wayland_tbm_client_destroy_buffer(wayland_display->wl_tbm_client, (void *)wayland_buffer->proc.app.wl_proxy);
		}

		buffer->backend.data = NULL;
		free(wayland_buffer);
	}
}

static void *
__tpl_wayland_buffer_map(tpl_buffer_t *buffer, int size)
{
	tpl_wayland_buffer_t *wayland_buffer;
	tbm_bo_handle handle;

	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.data);

	wayland_buffer = (tpl_wayland_buffer_t *) buffer->backend.data;

	TPL_ASSERT(wayland_buffer->bo);

	handle = tbm_bo_get_handle(wayland_buffer->bo, TBM_DEVICE_CPU);
	return handle.ptr;
}

static void
__tpl_wayland_buffer_unmap(tpl_buffer_t *buffer, void *ptr, int size)
{
	TPL_IGNORE(buffer);
	TPL_IGNORE(ptr);
	TPL_IGNORE(size);

	/* Do nothing. */
}

static tpl_bool_t
__tpl_wayland_buffer_lock(tpl_buffer_t *buffer, tpl_lock_usage_t usage)
{
	tpl_wayland_buffer_t *wayland_buffer;
	tbm_bo_handle handle;

	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.data);

	wayland_buffer = (tpl_wayland_buffer_t *) buffer->backend.data;

	TPL_ASSERT(wayland_buffer->bo);

	TPL_OBJECT_UNLOCK(buffer);

	switch (usage)
	{
		case TPL_LOCK_USAGE_GPU_READ:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_3D, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_GPU_WRITE:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_3D, TBM_OPTION_WRITE);
			break;
		case TPL_LOCK_USAGE_CPU_READ:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_CPU_WRITE:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
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
__tpl_wayland_buffer_unlock(tpl_buffer_t *buffer)
{
	tpl_wayland_buffer_t *wayland_buffer;

	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.data);

	wayland_buffer = (tpl_wayland_buffer_t *) buffer->backend.data;

	TPL_ASSERT(wayland_buffer->bo);

	TPL_OBJECT_UNLOCK(buffer);
	tbm_bo_unmap(wayland_buffer->bo);
	TPL_OBJECT_LOCK(buffer);
}

tpl_bool_t
__tpl_display_choose_backend_wayland(tpl_handle_t native_dpy)
{
	if (native_dpy == NULL)
		return TPL_FALSE;

	if (__tpl_wayland_display_is_wl_display(native_dpy))
		return TPL_TRUE;

	if (__tpl_wayland_display_is_gbm_device(native_dpy))
		return TPL_TRUE;

	return TPL_FALSE;
}

void
__tpl_display_init_backend_wayland(tpl_display_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init				= __tpl_wayland_display_init;
	backend->fini				= __tpl_wayland_display_fini;
	backend->query_config			= __tpl_wayland_display_query_config;
	backend->filter_config			= __tpl_wayland_display_filter_config;
	backend->get_window_info			= __tpl_wayland_display_get_window_info;
	backend->get_pixmap_info			= __tpl_wayland_display_get_pixmap_info;
	backend->flush				= __tpl_wayland_display_flush;
#ifdef EGL_BIND_WL_DISPLAY
	backend->bind_client_display_handle	= __tpl_wayland_display_bind_client_display;
	backend->unbind_client_display_handle	= __tpl_wayland_display_unbind_client_display;
#endif
}

void
__tpl_surface_init_backend_wayland(tpl_surface_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init		= __tpl_wayland_surface_init;
	backend->fini		= __tpl_wayland_surface_fini;
	backend->begin_frame	= __tpl_wayland_surface_begin_frame;
	backend->end_frame	= __tpl_wayland_surface_end_frame;
	backend->validate_frame	= __tpl_wayland_surface_validate_frame;
	backend->get_buffer	= __tpl_wayland_surface_get_buffer;
	backend->post		= __tpl_wayland_surface_post;
        backend->destroy_cached_buffers = __tpl_wayland_surface_destroy_cached_buffers;
	backend->update_cached_buffers = __tpl_wayland_surface_update_cached_buffers;
}

void
__tpl_buffer_init_backend_wayland(tpl_buffer_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init			= __tpl_wayland_buffer_init;
	backend->fini			= __tpl_wayland_buffer_fini;
	backend->map			= __tpl_wayland_buffer_map;
	backend->unmap			= __tpl_wayland_buffer_unmap;
	backend->lock			= __tpl_wayland_buffer_lock;
	backend->unlock			= __tpl_wayland_buffer_unlock;
	backend->create_native_buffer	= NULL;
}

static void
__cb_client_sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
	int *done;

	TPL_ASSERT(data);

	done = data;
	*done = 1;

	wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener =
{
	__cb_client_sync_callback
};

static void
__cb_client_frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	/* We moved the buffer reclaim logic to buffer_release_callback().
	   buffer_release_callback() is more suitable point to delete or reuse buffer instead of frame_callback().
	   But we remain this callback because buffer_release_callback() works only when frame_callback() is activated.*/
	TPL_IGNORE(data);
	TPL_IGNORE(time);

	wl_callback_destroy(callback);
}

static const struct wl_callback_listener frame_listener =
{
	__cb_client_frame_callback
};

static void
__cb_client_buffer_release_callback(void *data, struct wl_proxy *proxy)
{
	tpl_buffer_t *tpl_buffer;
	tpl_surface_t *surface;

	TPL_ASSERT(data);

	tpl_buffer = (tpl_buffer_t *) data;
	surface = tpl_buffer->surface;
	tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;

	TPL_LOG(3, "release window(%p, %p), buffer(%p), key:%zu",
		surface, surface?surface->native_handle:NULL,
		tpl_buffer, tpl_buffer->key);

	if (surface != NULL)
	{
		TPL_OBJECT_LOCK(surface);

		tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t*) tpl_buffer->backend.data;
		wayland_buffer->status = IDLE;
		tbm_surface_internal_unref(wayland_buffer->tbm_surface);
		tbm_surface_queue_release(wayland_surface->tbm_queue, wayland_buffer->tbm_surface);
		TPL_LOG(7, "BO:%d", tbm_bo_export(wayland_buffer->bo));

		TPL_OBJECT_UNLOCK(surface);
	}

	tpl_object_unreference((tpl_object_t *)tpl_buffer);
}

static const struct wl_buffer_listener buffer_release_listener = {
	(void *)__cb_client_buffer_release_callback,
};

#ifdef EGL_BIND_WL_DISPLAY
unsigned int __tpl_wayland_display_bind_client_display(tpl_display_t *tpl_display, tpl_handle_t native_dpy)
{
	tpl_wayland_display_t *tpl_wayland_display;

	TPL_ASSERT(tpl_display);
	TPL_ASSERT(native_dpy);

	tpl_wayland_display = (tpl_wayland_display_t *) tpl_display->backend.data;
	tpl_display->bufmgr_fd = dup(gbm_device_get_fd(tpl_display->native_handle));
	tpl_wayland_display->bufmgr = tbm_bufmgr_init(tpl_display->bufmgr_fd);
	if (tpl_wayland_display->bufmgr == NULL)
	{
		TPL_ERR("TBM buffer manager initialization failed!");
		return TPL_FALSE;
	}

	tpl_wayland_display->proc.comp.bound_client_display = TPL_TRUE;
	return TPL_TRUE;
}

unsigned int __tpl_wayland_display_unbind_client_display(tpl_display_t *tpl_display, tpl_handle_t native_dpy)
{
	tpl_wayland_display_t *tpl_wayland_display;

	TPL_ASSERT(tpl_display);

	tpl_wayland_display = (tpl_wayland_display_t *) tpl_display->backend.data;

	tbm_bufmgr_deinit(tpl_wayland_display->bufmgr);
 	close(tpl_display->bufmgr_fd);
	tpl_wayland_display->proc.comp.bound_client_display = TPL_FALSE;
	return TPL_TRUE;
}
#endif
