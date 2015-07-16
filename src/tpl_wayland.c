#define inline __inline__

#include <wayland-drm.h>
#include <wayland-egl-priv.h>

#include <wayland-client.h>
#include <wayland-drm-client-protocol.h>

#include <drm.h>
#include <tbm_bufmgr.h>
#include <gbm.h>
#include <gbm_tbm.h>
#include <xf86drm.h>

#undef inline

#include "tpl_internal.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* In wayland, application and compositor create its own drawing buffers. Recommend size is more than 2. */
#define TPL_BUFFER_ALLOC_SIZE_APP               3
#define TPL_BUFFER_ALLOC_SIZE_COMPOSITOR        4

#define TPL_BUFFER_ALLOC_PITCH_ALIGNMENT        64
#define ALIGN_TO_64BYTE(byte) (((byte) + TPL_BUFFER_ALLOC_PITCH_ALIGNMENT - 1) & ~(TPL_BUFFER_ALLOC_PITCH_ALIGNMENT - 1))

typedef struct _tpl_wayland_display       tpl_wayland_display_t;
typedef struct _tpl_wayland_surface       tpl_wayland_surface_t;
typedef struct _tpl_wayland_buffer        tpl_wayland_buffer_t;

struct _tpl_wayland_display
{
	struct wl_drm     *wl_drm;
	tbm_bufmgr         bufmgr;

	union
	{
		struct
		{
			tpl_bool_t               authenticated;
			struct wl_event_queue   *wl_queue;
			struct wl_registry      *wl_registry;
		} app;
		struct
		{
			tpl_list_t               cached_buffers;
		} comp;
	} proc;
};

struct _tpl_wayland_surface
{
	tpl_list_t        able_rendering_queue;
	tpl_buffer_t     *current_rendering_buffer;
	tpl_list_t        done_rendering_queue;
};

struct _tpl_wayland_buffer
{
	tpl_display_t             *display;
	tbm_bo                     bo;
	int			   reused;

	union
	{
		struct
		{
			struct wl_resource      *wl_resource;
			tpl_bool_t               resized;
		} app;
		struct
		{
			struct gbm_bo           *gbm_bo;
			tpl_bool_t               posted;
		} comp;
	} proc;
};

static const struct wl_registry_listener registry_listener;
static const struct wl_callback_listener sync_listener;
static const struct wl_callback_listener frame_listener;
static const struct wl_buffer_listener buffer_release_listener;

static struct gbm_bo *__cb_server_gbm_surface_lock_front_buffer(struct gbm_surface *gbm_surf);
static void __cb_server_gbm_surface_release_buffer(struct gbm_surface *gbm_surf, struct gbm_bo *gbm_bo);
static int __cb_server_gbm_surface_has_free_buffers(struct gbm_surface *gbm_surf);

#define TPL_BUFFER_CACHE_MAX_ENTRIES 40
static TPL_INLINE void
__tpl_wayland_surface_buffer_cache_add(tpl_list_t *buffer_cache, tpl_buffer_t *buffer)
{
	tpl_buffer_t *evict = NULL;

	if (tpl_list_get_count(buffer_cache) >= TPL_BUFFER_CACHE_MAX_ENTRIES)
	{
		evict = tpl_list_pop_front(buffer_cache, NULL);
		tpl_object_unreference(&evict->base);
	}

	tpl_object_reference(&buffer->base);
	tpl_list_push_back(buffer_cache, (void *)buffer);

	TPL_LOG(3, "buf:%10p buf->base:%10p evict:%10p", buffer, &buffer->base, evict);
}

static TPL_INLINE void
__tpl_wayland_surface_buffer_cache_remove(tpl_list_t	 *buffer_cache, unsigned int name)
{
	tpl_list_node_t *node = tpl_list_get_front_node(buffer_cache);

	while (node)
	{
		tpl_buffer_t *buffer = (tpl_buffer_t *)tpl_list_node_get_data(node);

		if (buffer->key == name)
		{
			tpl_object_unreference(&buffer->base);
			tpl_list_remove(node, NULL);
			TPL_LOG(3, "name:%d buf:%10p buf->base:%10p", name, buffer, &buffer->base);
			return;
		}

		node = tpl_list_node_next(node);
	}

	TPL_LOG(3, "Buffer named %d not found in cache", name);
}

static TPL_INLINE tpl_buffer_t *
__tpl_wayland_surface_buffer_cache_find(tpl_list_t	 *buffer_cache, unsigned int name)
{
	tpl_list_node_t *node = tpl_list_get_front_node(buffer_cache);

	while (node)
	{
		tpl_buffer_t *buffer = (tpl_buffer_t *)tpl_list_node_get_data(node);

		if (buffer->key == name)
		{
			TPL_LOG(3, "name:%d buf:%10p buf->base:%10p", name, buffer, &buffer->base);
			return buffer;
		}

		node = tpl_list_node_next(node);
	}

	TPL_LOG(3, "Buffer named %d not found in cache", name);

	return NULL;
}


static TPL_INLINE tpl_bool_t
__tpl_wayland_display_is_wl_display(tpl_handle_t native_dpy)
{
	/* MAGIC CHECK: A native display handle is a wl_display if the de-referenced first value
	   is a memory address pointing the structure of wl_display_interface. */
	if (*(void **)native_dpy == &wl_display_interface) return TPL_TRUE;
	return TPL_FALSE;
}

static TPL_INLINE tpl_bool_t
__tpl_wayland_display_is_gbm_device(tpl_handle_t native_dpy)
{
	/* MAGIC CHECK: A native display handle is a gbm_device if the de-referenced first value
	   is a memory address pointing gbm_create_surface(). */
	if (*(void **)native_dpy == gbm_create_device) return TPL_TRUE;
	return TPL_FALSE;
}

static int
__tpl_wayland_display_roundtrip(tpl_display_t *display)
{
	struct wl_display *wl_dpy = (struct wl_display *)display->native_handle;
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;
	struct wl_callback *callback;
	int done = 0, ret = 0;

	callback = wl_display_sync(wl_dpy);
	wl_callback_add_listener(callback, &sync_listener, &done);
	wl_proxy_set_queue((struct wl_proxy *)callback, wayland_display->proc.app.wl_queue);
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

	/* Do not allow default display in wayland. */
	if (display->native_handle == NULL)
		return TPL_FALSE;

	wayland_display = (tpl_wayland_display_t *)calloc(1, sizeof(tpl_wayland_display_t));
	if (wayland_display == NULL)
		return TPL_FALSE;

	display->backend.data = wayland_display;

	display->bufmgr_fd = -1;

	if (__tpl_wayland_display_is_wl_display(display->native_handle))
	{
		struct wl_display *wl_dpy = (struct wl_display *)display->native_handle;

		wayland_display->proc.app.wl_queue = wl_display_create_queue(wl_dpy);
		wayland_display->proc.app.wl_registry = wl_display_get_registry(wl_dpy);
		wl_proxy_set_queue((struct wl_proxy *)wayland_display->proc.app.wl_registry, wayland_display->proc.app.wl_queue);
		wl_registry_add_listener(wayland_display->proc.app.wl_registry, &registry_listener, display);

		/* Initialization roundtrip steps */
		if (__tpl_wayland_display_roundtrip(display) < 0 || wayland_display->wl_drm == NULL) goto error;
		if (__tpl_wayland_display_roundtrip(display) < 0 || display->bufmgr_fd == -1) goto error;
		if (__tpl_wayland_display_roundtrip(display) < 0 || wayland_display->proc.app.authenticated == TPL_FALSE) goto error;

		wayland_display->bufmgr = tbm_bufmgr_init(display->bufmgr_fd);
	}
	else if (__tpl_wayland_display_is_gbm_device(display->native_handle))
	{
		struct gbm_device *gbm = (struct gbm_device *)display->native_handle;
		struct gbm_tbm_device *gbm_tbm = (struct gbm_tbm_device *)gbm;

		/* Hook gbm backend callbacks. If the compositor calls gbm APIs to get a buffer,
		   then we return a suitable buffer to the compositor instead of gbm does. */
		gbm_tbm_device_set_callback_surface_has_free_buffers(gbm_tbm, __cb_server_gbm_surface_has_free_buffers);
		gbm_tbm_device_set_callback_surface_lock_front_buffer(gbm_tbm, __cb_server_gbm_surface_lock_front_buffer);
		gbm_tbm_device_set_callback_surface_release_buffer(gbm_tbm, __cb_server_gbm_surface_release_buffer);

		tpl_list_init(&wayland_display->proc.comp.cached_buffers);
	}
	else
		goto error;

	return TPL_TRUE;

error:
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
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;

	if (wayland_display != NULL)
	{
		if (__tpl_wayland_display_is_wl_display(display->native_handle))
		{
			tbm_bufmgr_deinit(wayland_display->bufmgr);
			close(display->bufmgr_fd);
		}
		if (__tpl_wayland_display_is_gbm_device(display->native_handle))
		{
			struct gbm_device *gbm = (struct gbm_device *)display->native_handle;
			struct gbm_tbm_device *gbm_tbm = (struct gbm_tbm_device *)gbm;

			gbm_tbm_device_set_callback_surface_has_free_buffers(gbm_tbm, NULL);
			gbm_tbm_device_set_callback_surface_lock_front_buffer(gbm_tbm, NULL);
			gbm_tbm_device_set_callback_surface_release_buffer(gbm_tbm, NULL);

			tpl_list_fini(&wayland_display->proc.comp.cached_buffers, (tpl_free_func_t) tpl_object_unreference);
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
	if (surface_type == TPL_SURFACE_TYPE_WINDOW &&
		red_size == 8 &&
		green_size == 8 &&
		blue_size == 8 &&
		(color_depth == 32 || color_depth == 24))
	{
		if (alpha_size == 8)
		{
			if (__tpl_wayland_display_is_wl_display(display->native_handle))
			{
				if (native_visual_id != NULL) *native_visual_id = WL_DRM_FORMAT_ARGB8888;
			}
			else if (__tpl_wayland_display_is_gbm_device(display->native_handle) &&
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
			if (__tpl_wayland_display_is_wl_display(display->native_handle))
			{
				if (native_visual_id != NULL) *native_visual_id = WL_DRM_FORMAT_XRGB8888;
			}
			else if (__tpl_wayland_display_is_gbm_device(display->native_handle) &&
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

	if (*visual_id == GBM_FORMAT_ARGB8888 && alpha_size == 0)
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
	if (__tpl_wayland_display_is_wl_display(display->native_handle))
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
				*format = TPL_FORMAT_ARGB8888;
		}
		if (width != NULL) *width = wl_egl_window->width;
		if (height != NULL) *height = wl_egl_window->height;

		return TPL_TRUE;
	}
	else if (__tpl_wayland_display_is_gbm_device(display->native_handle) == TPL_TRUE)
	{
		struct gbm_surface *gbm_surface = (struct gbm_surface *)window;
		struct gbm_tbm_surface *gbm_tbm_surface = (struct gbm_tbm_surface *)gbm_surface;

		if (format != NULL)
		{
			switch (gbm_tbm_surface_get_format(gbm_tbm_surface))
			{
				case GBM_FORMAT_ARGB8888: *format = TPL_FORMAT_ARGB8888; break;
				case GBM_FORMAT_XRGB8888: *format = TPL_FORMAT_XRGB8888; break;
				case GBM_FORMAT_RGB565: *format = TPL_FORMAT_RGB565; break;
				default: *format = TPL_FORMAT_INVALID; break;
			}
		}
		if (width != NULL) *width = gbm_tbm_surface_get_width(gbm_tbm_surface);
		if (height != NULL) *height = gbm_tbm_surface_get_height(gbm_tbm_surface);

		return TPL_TRUE;
	}

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_wayland_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
				      int *width, int *height, tpl_format_t *format)
{
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;
	struct wl_drm_buffer *drm_buffer = NULL;

	if (wayland_display->wl_drm == NULL)
		return TPL_FALSE;

	drm_buffer = wayland_drm_buffer_get(wayland_display->wl_drm, (struct wl_resource *)pixmap);

	if (drm_buffer != NULL)
	{
		if (format != NULL)
		{
			switch (drm_buffer->format)
			{
				case WL_DRM_FORMAT_ARGB8888: *format = TPL_FORMAT_ARGB8888; break;
				case WL_DRM_FORMAT_XRGB8888: *format = TPL_FORMAT_XRGB8888; break;
				case WL_DRM_FORMAT_RGB565: *format = TPL_FORMAT_RGB565; break;
				default: *format = TPL_FORMAT_INVALID; break;
			}
		}
		if (width != NULL) *width = drm_buffer->width;
		if (height != NULL) *height = drm_buffer->height;

		return TPL_TRUE;
	}

	return TPL_FALSE;
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
	int i;

	wayland_surface = (tpl_wayland_surface_t *)calloc(1, sizeof(tpl_wayland_surface_t));
	TPL_CHECK_ON_NULL_RETURN_VAL(wayland_surface, TPL_FALSE);

	surface->backend.data = (void *)wayland_surface;

	tpl_list_init(&wayland_surface->able_rendering_queue);
	tpl_list_init(&wayland_surface->done_rendering_queue);

	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		if (__tpl_wayland_display_is_wl_display(surface->display->native_handle))
		{
			struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)surface->native_handle;
			wl_egl_window->private = surface;

			/* Create renderable buffer queue. Fill with empty(=NULL) buffers. */
			for (i = 0; i < TPL_BUFFER_ALLOC_SIZE_APP; i++)
			{
				tpl_list_push_back(&wayland_surface->able_rendering_queue, NULL);
			}
		}
		if (__tpl_wayland_display_is_gbm_device(surface->display->native_handle))
		{
			struct gbm_surface *gbm_surface = surface->native_handle;
			struct gbm_tbm_surface *gbm_tbm_surface = (struct gbm_tbm_surface *)gbm_surface;
			gbm_tbm_surface_set_user_data(gbm_tbm_surface, surface);

			/* Create renderable buffer queue. Fill with empty(=NULL) buffers. */
			for (i = 0; i < TPL_BUFFER_ALLOC_SIZE_COMPOSITOR; i++)
			{
				tpl_list_push_back(&wayland_surface->able_rendering_queue, NULL);
			}
		}

		__tpl_wayland_display_get_window_info(surface->display, surface->native_handle,
			&surface->width, &surface->height, NULL, 0, 0);

		TPL_LOG(3, "window(%p, %p) %dx%d", surface, surface->native_handle, surface->width, surface->height);
		return TPL_TRUE;
	}

	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		__tpl_wayland_display_get_pixmap_info(surface->display, surface->native_handle,
						  &surface->width, &surface->height, NULL);
		return TPL_TRUE;
	}

	return TPL_FALSE;
}

static void
__tpl_wayland_surface_buffer_free(tpl_buffer_t *buffer)
{
	TPL_LOG(3, "buffer(%p) key:%d", buffer, buffer?buffer->key:-1);
	if (buffer != NULL)
	{
		__tpl_buffer_set_surface(buffer, NULL);
		tpl_object_unreference((tpl_object_t *)buffer);
	}
}

static void
__tpl_wayland_surface_fini(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;

	TPL_CHECK_ON_NULL_RETURN(wayland_surface);
	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	while (!tpl_list_is_empty(&surface->frame_queue))
	{
		tpl_util_sys_yield();
	}

	if (wayland_surface != NULL)
	{
		TPL_LOG(3, "free buffers able(%d), current:%d, done:%d",
				tpl_list_get_count(&wayland_surface->able_rendering_queue),
				wayland_surface->current_rendering_buffer?1:0,
				tpl_list_get_count(&wayland_surface->done_rendering_queue));

		tpl_list_fini(&wayland_surface->able_rendering_queue, (tpl_free_func_t)__tpl_wayland_surface_buffer_free);
		__tpl_wayland_surface_buffer_free(wayland_surface->current_rendering_buffer);
		tpl_list_fini(&wayland_surface->done_rendering_queue, (tpl_free_func_t)__tpl_wayland_surface_buffer_free);

		if (surface->type == TPL_SURFACE_TYPE_WINDOW) 
		{
			if (__tpl_wayland_display_is_wl_display(surface->display->native_handle))
			{
				struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)surface->native_handle;
				wl_egl_window->private = NULL;

				/* Detach all pending buffers */
				if (wl_egl_window->surface && wl_egl_window->width == wl_egl_window->attached_width &&
					wl_egl_window->height == wl_egl_window->attached_height)
				{
					wl_surface_attach(wl_egl_window->surface, NULL, 0, 0);
					wl_surface_commit(wl_egl_window->surface);
				}

				wl_display_flush(surface->display->native_handle);
				__tpl_wayland_display_roundtrip(surface->display);
			}

			if (__tpl_wayland_display_is_gbm_device(surface->display->native_handle))
			{
				struct gbm_surface *gbm_surface = surface->native_handle;
				struct gbm_tbm_surface *gbm_tbm_surface = (struct gbm_tbm_surface *)gbm_surface;

				gbm_tbm_surface_set_user_data(gbm_tbm_surface, NULL);
			}
		}

		free(wayland_surface);
	}
	surface->backend.data = NULL;
}

static void
__tpl_wayland_surface_post(tpl_surface_t *surface, tpl_frame_t *frame)
{
	TPL_ASSERT(frame->buffer != NULL);
	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (__tpl_wayland_display_is_wl_display(surface->display->native_handle))
	{
		tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)surface->display->backend.data;
		tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)frame->buffer->backend.data;
		struct wl_egl_window *wl_egl_window = NULL;
		int i;

		TPL_LOG(3, "\t buffer(%p, %p) key:%d", frame->buffer, wayland_buffer->proc.app.wl_resource, frame->buffer->key);
		wl_egl_window = (struct wl_egl_window *)surface->native_handle;
		tpl_object_reference((tpl_object_t *)frame->buffer);
		wl_surface_attach(wl_egl_window->surface,
			(void *)wayland_buffer->proc.app.wl_resource,
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
	}
	if (__tpl_wayland_display_is_gbm_device(surface->display->native_handle))
	{
		tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)frame->buffer->backend.data;

		wayland_buffer->proc.comp.posted = TPL_TRUE;
	}
}

static void
__tpl_wayland_surface_begin_frame(tpl_surface_t *surface)
{
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)surface->display->backend.data;
	tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;

	TPL_ASSERT(wayland_surface->current_rendering_buffer == NULL);
	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (__tpl_wayland_display_is_wl_display(surface->display->native_handle))
	{
		TPL_OBJECT_UNLOCK(surface);

		__tpl_wayland_display_roundtrip(surface->display);

		while (tpl_list_is_empty(&wayland_surface->able_rendering_queue))
		{
			/* Application sent all buffers to the server. Wait for server response. */
			if (wl_display_dispatch_queue(surface->display->native_handle, wayland_display->proc.app.wl_queue) == -1)
			{
				TPL_OBJECT_LOCK(surface);
				return;
			}
		}

		TPL_OBJECT_LOCK(surface);
	}
	if (__tpl_wayland_display_is_gbm_device(surface->display->native_handle))
	{
		while (1)
		{
			if (!tpl_list_is_empty(&wayland_surface->able_rendering_queue))
			{
				tpl_buffer_t *buffer = NULL;
				tpl_wayland_buffer_t *wayland_buffer = NULL;

				buffer = tpl_list_get_front(&wayland_surface->able_rendering_queue);
				if (buffer == NULL) break;

				wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;
				if (wayland_buffer->proc.comp.posted) break;
			}
			/* Compositor over-drawed all buffers, but no buffer has done yet. Wait for frame post. */
			TPL_OBJECT_UNLOCK(surface);
			tpl_util_sys_yield();
			TPL_OBJECT_LOCK(surface);
		}
	}
	/* MOVE BUFFER : [able queue] --> (current buffer) */
	wayland_surface->current_rendering_buffer = tpl_list_pop_front(&wayland_surface->able_rendering_queue, NULL);
	TPL_LOG(3, "set current buffer(%p)", wayland_surface->current_rendering_buffer);
}

static tpl_bool_t
__tpl_wayland_surface_validate_frame(tpl_surface_t *surface)
{
	TPL_IGNORE(surface);

	return TPL_TRUE;
}

static void
__tpl_wayland_surface_end_frame(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (__tpl_wayland_display_is_gbm_device(surface->display->native_handle))
	{
		tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)wayland_surface->current_rendering_buffer->backend.data;

		/* Current GBM front buffer is moved to back when calling eglSwapBuffers()=end_frame(). */
		if (!tpl_list_is_empty(&wayland_surface->done_rendering_queue))
		{
			/* MOVE BUFFER : [done queue] --> [able queue] */
			tpl_list_push_back(&wayland_surface->able_rendering_queue, tpl_list_pop_front(&wayland_surface->done_rendering_queue, NULL));
		}

		/* Prepare to check post for current GBM back buffer. */
		wayland_buffer->proc.comp.posted = TPL_FALSE;
	}

	/* MOVE BUFFER : (current buffer) --> [done queue] */
	tpl_list_push_back(&wayland_surface->done_rendering_queue, wayland_surface->current_rendering_buffer);
	wayland_surface->current_rendering_buffer = NULL;

}

static tpl_buffer_t *
__tpl_wayland_surface_create_buffer_from_wl_egl(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tbm_bo bo;
	tbm_bo_handle bo_handle;
	int width, height, depth, stride;
	tpl_format_t format;

	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)surface->display->backend.data;
	struct wl_resource *wl_resource = NULL;
	unsigned int name = -1;
	uint32_t wl_format = 0;

	__tpl_wayland_display_get_window_info(surface->display, surface->native_handle,
					  &width, &height, &format, 0, 0);

	depth = 32;//TPL_FORMAT_GET_DEPTH(format);

	stride = ALIGN_TO_64BYTE(width * depth / 8);

	/* Allocate a buffer */
	bo = tbm_bo_alloc(wayland_display->bufmgr, stride * height, TBM_BO_DEFAULT);
	TPL_CHECK_ON_NULL_RETURN_VAL(bo, NULL);

	/* Create tpl buffer. */
	bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);

	
	name = tbm_bo_export(bo);
	buffer = __tpl_buffer_alloc(surface, (int)name, (int)bo_handle.u32, width, height, depth, stride);
	TPL_CHECK_ON_NULL_RETURN_VAL(buffer, NULL);

	wayland_buffer = (tpl_wayland_buffer_t *)calloc(1, sizeof(tpl_wayland_buffer_t));
	if (wayland_buffer == NULL)
	{
		TPL_ERR("wayland_buffer==NULL");

		tbm_bo_unref(bo);
		tpl_object_unreference((tpl_object_t *)buffer);
		return NULL;
	}
	buffer->backend.data = (void *)wayland_buffer;
	surface->format = TPL_FORMAT_ARGB8888;
	/* Post process : Create a wl_drm_buffer and notify the buffer to the server. */
	switch (surface->format)
	{
		case TPL_FORMAT_ARGB8888: wl_format = WL_DRM_FORMAT_ARGB8888; break;
		case TPL_FORMAT_XRGB8888: wl_format = WL_DRM_FORMAT_XRGB8888; break;
		case TPL_FORMAT_RGB565: wl_format = WL_DRM_FORMAT_RGB565; break;
		default:
			TPL_ERR("surface->format==Unknown");

			tbm_bo_unref(bo);
			tpl_object_unreference((tpl_object_t *)buffer);
			return NULL;
	}

	wl_resource = (struct wl_resource *)wl_drm_create_buffer(wayland_display->wl_drm, (uint32_t)name, width, height,
										stride, wl_format);

	wl_proxy_set_queue((struct wl_proxy *)wl_resource, wayland_display->proc.app.wl_queue);
	wl_buffer_add_listener((void *)wl_resource, &buffer_release_listener, buffer);

	wl_display_flush((struct wl_display *)surface->display->native_handle);

	wayland_buffer->display = surface->display;
	wayland_buffer->bo = bo;
	wayland_buffer->proc.app.wl_resource = wl_resource;
	wayland_buffer->proc.app.resized = TPL_FALSE;

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "buffer(%p,%p) name:%d, %dx%d", buffer, wl_resource, name, width, height);
	return buffer;
}

static tpl_buffer_t *
__tpl_wayland_surface_create_buffer_from_gbm_surface(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tbm_bo bo;
	tbm_bo_handle bo_handle;
	int width, height, depth, stride;
	tpl_format_t format;

	struct gbm_device *gbm = (struct gbm_device *)surface->display->native_handle;
	struct gbm_surface *gbm_surface = surface->native_handle;
	struct gbm_tbm_surface *gbm_tbm_surface = (struct gbm_tbm_surface *)gbm_surface;
	struct gbm_bo *gbm_bo = NULL;
	struct gbm_tbm_bo *gbm_tbm_bo = NULL;

	__tpl_wayland_display_get_window_info(surface->display, surface->native_handle,
					  &width, &height, &format, 0, 0);

	depth = 32;//TPL_FORMAT_GET_DEPTH(format);

	stride = ALIGN_TO_64BYTE(width * depth / 8);

	/* gbm does not support stride so we must ensure the width is same as stride. */
	if (width > 1 && (width * depth / 8 != stride) )
	{
		TPL_WARN("Unsupported stride %d", stride);
		return NULL;
	}

	/* Allocate a buffer */
	gbm_bo = gbm_bo_create(gbm, width, height,
			       gbm_tbm_surface_get_format(gbm_tbm_surface),
			       gbm_tbm_surface_get_flags(gbm_tbm_surface));

	if (gbm_bo == NULL)
	{
		TPL_WARN("Failed to allocate gbm_bo | gbm:%p %dx%d", gbm, width, height);
		return NULL;
	}

	gbm_tbm_bo = (struct gbm_tbm_bo *)(gbm_bo);
	bo = tbm_bo_ref(gbm_tbm_bo_get_tbm_bo(gbm_tbm_bo));

	/* Create tpl buffer. */
	bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);

	buffer = __tpl_buffer_alloc(surface, (int)bo_handle.u32, 
	                  (int)bo_handle.u32, width, height, depth, stride);
	if (buffer == NULL)
	{
		tbm_bo_unref(bo);
		TPL_WARN("Failed to allocate tpl buffer | surf:%p bo_hnd:%d WxHxD:%dx%dx%d",
			surface, (int) bo_handle.u32, width, height, depth);
		return NULL;
	}

	wayland_buffer = (tpl_wayland_buffer_t *)calloc(1, sizeof(tpl_wayland_buffer_t));
	if (wayland_buffer == NULL)
	{
		tbm_bo_unref(bo);
		tpl_object_unreference((tpl_object_t *)buffer);
		TPL_WARN("Failed to allocate wayland buffer (calloc)");
		return NULL;
	}
	buffer->backend.data = (void *)wayland_buffer;

	/* Post process */
	wayland_buffer->display = surface->display;
	wayland_buffer->bo = bo;
	wayland_buffer->proc.comp.gbm_bo = gbm_bo;

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "buffer:%p gbm_bo:%p bo_hnd:%d, %dx%d", buffer, gbm_bo, (int) bo_handle.u32, width, height);

	return buffer;
}

static tpl_buffer_t *
__tpl_wayland_surface_create_buffer_from_wl_drm(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tbm_bo bo;
	tbm_bo_handle bo_handle;
	int width = 0, height = 0, depth, stride;
	tpl_format_t format = TPL_FORMAT_INVALID;
	unsigned int key = 0;

	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)surface->display->backend.data;
	struct wl_drm_buffer *drm_buffer = NULL;

	TPL_ASSERT(wayland_display->wl_drm != NULL);

	/* Get the allocated buffer */
	drm_buffer = wayland_drm_buffer_get(wayland_display->wl_drm, (struct wl_resource *)surface->native_handle);

	buffer = __tpl_wayland_surface_buffer_cache_find(&wayland_display->proc.comp.cached_buffers, (unsigned int) drm_buffer);
	if (buffer != NULL)
	{
		__tpl_buffer_set_surface(buffer, surface);
		tpl_object_reference((tpl_object_t *)buffer);
	}
	else
	{
		__tpl_wayland_display_get_pixmap_info(surface->display, surface->native_handle,
						  &width, &height, &format);

		depth = 32;//TPL_FORMAT_GET_DEPTH(format);

		stride = drm_buffer->stride[0];

		bo = tbm_bo_ref((tbm_bo)wayland_drm_buffer_get_buffer(drm_buffer));

		/* Create tpl buffer. */
		bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);
		key = (unsigned int)drm_buffer;
		buffer = __tpl_buffer_alloc(surface, key, 
		                  (int)bo_handle.u32, width, height, depth, stride);
		if (buffer == NULL)
		{
			tbm_bo_unref(bo);
			return NULL;
		}

		wayland_buffer = (tpl_wayland_buffer_t *)calloc(1, sizeof(tpl_wayland_buffer_t));
		if (wayland_buffer == NULL)
		{
			tbm_bo_unref(bo);
			tpl_object_unreference((tpl_object_t *)buffer);
			return NULL;
		}
		buffer->backend.data = (void *)wayland_buffer;

		/* Post process */
		wayland_buffer->display = surface->display;
		wayland_buffer->bo = bo;
		buffer->key = key;
		__tpl_wayland_surface_buffer_cache_add(&wayland_display->proc.comp.cached_buffers, buffer); /* TODO: do we need error handle? */
	}

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	return buffer;
}

static tpl_buffer_t *
__tpl_wayland_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;
	int i;

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	TPL_LOG(3, "window(%p, %p), current(%p)", surface, surface->native_handle,
		wayland_surface->current_rendering_buffer);

	if (wayland_surface->current_rendering_buffer == NULL)
	{
		if (surface->type == TPL_SURFACE_TYPE_WINDOW)
		{
			if (__tpl_wayland_display_is_wl_display(surface->display->native_handle))
			{
				wayland_surface->current_rendering_buffer =
					__tpl_wayland_surface_create_buffer_from_wl_egl(surface, reset_buffers);
			}
			if (__tpl_wayland_display_is_gbm_device(surface->display->native_handle))
			{
				wayland_surface->current_rendering_buffer =
					__tpl_wayland_surface_create_buffer_from_gbm_surface(surface, reset_buffers);
			}
		}
		if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
		{
			wayland_surface->current_rendering_buffer =
				__tpl_wayland_surface_create_buffer_from_wl_drm(surface, reset_buffers);
		}
	}
	else
	{	
		int reused = 1;

		if (surface->type == TPL_SURFACE_TYPE_WINDOW &&
			__tpl_wayland_display_is_wl_display(surface->display->native_handle))
		{
			int width, height;

			__tpl_wayland_display_get_window_info(surface->display, surface->native_handle,
							      &width, &height, NULL, 0, 0);

			/* Check whether the surface was resized by wayland_egl */
			if (width != wayland_surface->current_rendering_buffer->width ||
			    height != wayland_surface->current_rendering_buffer->height)
			{
				int count;
				TPL_LOG(3, "window(%p, %p) size changed: %dx%d -> %dx%d", surface, surface->native_handle,
						wayland_surface->current_rendering_buffer->width, wayland_surface->current_rendering_buffer->height,
						width, height);

				/* Marks 'resized' on done queue. Rendered buffers can be used by server(other process).
				   So these buffers will be destroyed when reuse.  */
				tpl_list_node_t *node = tpl_list_get_front_node(&wayland_surface->done_rendering_queue);
				while (node != NULL)
				{
					tpl_buffer_t *node_buffer = (tpl_buffer_t *)tpl_list_node_get_data(node);
					tpl_wayland_buffer_t *node_wayland_buffer = (tpl_wayland_buffer_t *)node_buffer->backend.data;

					node_wayland_buffer->proc.app.resized = TPL_TRUE;
					node = tpl_list_node_next(node);
				}

				/* Throw away all able queue. (these are completely free buffers.)
				   And reconstruct renderable buffer queue */
				count = tpl_list_get_count(&wayland_surface->able_rendering_queue);
				tpl_list_fini(&wayland_surface->able_rendering_queue, (tpl_free_func_t)__tpl_wayland_surface_buffer_free);
				for (i = 0; i < count; i++)
				{
					tpl_list_push_back(&wayland_surface->able_rendering_queue, NULL);
				}

				/* Replace current current buffer */
				TPL_LOG(3, "free current buffer(%p)", wayland_surface->current_rendering_buffer);
				__tpl_wayland_surface_buffer_free(wayland_surface->current_rendering_buffer);
				wayland_surface->current_rendering_buffer =
					__tpl_wayland_surface_create_buffer_from_wl_egl(surface, reset_buffers);

				reused = 0;
				if (reset_buffers != NULL)
					*reset_buffers = TPL_TRUE;

			}
		}

		if (surface->type == TPL_SURFACE_TYPE_WINDOW)		
		{
			tpl_wayland_buffer_t *tpl_wayland_buffer;
		
			tpl_wayland_buffer = (tpl_wayland_buffer_t *)wayland_surface->current_rendering_buffer->backend.data;
			tpl_wayland_buffer->reused = reused; 		
			
		}
	}

	TPL_LOG(3, "\t buffer(%p) key:%d", wayland_surface->current_rendering_buffer, wayland_surface->current_rendering_buffer->key);
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
	TPL_LOG(3, "tpl_buffer(%p) key:%d fd:%d %dx%d", buffer, buffer->key, buffer->fd, buffer->width, buffer->height);
	
	if (buffer->backend.data)
	{
		tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;

		if (wayland_buffer->bo != NULL)
		{
			tbm_bo_unref(wayland_buffer->bo);
			wayland_buffer->bo = NULL;
		}

		if (__tpl_wayland_display_is_wl_display(wayland_buffer->display->native_handle))
		{
			wl_display_flush((struct wl_display *)wayland_buffer->display->native_handle);

			if (wayland_buffer->proc.app.wl_resource != NULL)
				wl_buffer_destroy((void *)wayland_buffer->proc.app.wl_resource);
		}
		if (__tpl_wayland_display_is_gbm_device(wayland_buffer->display->native_handle))
		{
			if (wayland_buffer->proc.comp.gbm_bo != NULL)
			{
				gbm_bo_destroy(wayland_buffer->proc.comp.gbm_bo);
			}
		}

		buffer->backend.data = NULL;
		free(wayland_buffer);
	}
}

static void *
__tpl_wayland_buffer_map(tpl_buffer_t *buffer, int size)
{
	tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;
	tbm_bo_handle handle;

	TPL_ASSERT(wayland_buffer != NULL);
	TPL_ASSERT(wayland_buffer->bo != NULL);

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
	tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;
	tbm_bo_handle handle;

	TPL_ASSERT(wayland_buffer != NULL);
	TPL_ASSERT(wayland_buffer->bo != NULL);

	switch (usage)
	{
		case TPL_LOCK_USAGE_GPU_READ:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_MM, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_GPU_WRITE:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_MM, TBM_OPTION_WRITE);
			break;
		case TPL_LOCK_USAGE_CPU_READ:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_CPU_WRITE:
			handle = tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
			break;
		default:
			TPL_ASSERT(TPL_FALSE);
			return TPL_FALSE;
	}

	if (handle.u32 != 0 || handle.ptr != NULL)
		return TPL_FALSE;

	return TPL_TRUE;
}

static void
__tpl_wayland_buffer_unlock(tpl_buffer_t *buffer)
{
	tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;

	TPL_ASSERT(wayland_buffer != NULL);
	TPL_ASSERT(wayland_buffer->bo != NULL);

	tbm_bo_unmap(wayland_buffer->bo);
}

static void *
__tpl_wayland_buffer_create_native_buffer(tpl_buffer_t *buffer)
{
	tpl_surface_t *surface = buffer->surface;
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)surface->display->backend.data;
	tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;
	struct wl_resource *wl_resource = NULL;
	uint32_t wl_format = 0;
	unsigned int name = 0;

	if (wayland_display->wl_drm == NULL)
		return TPL_FALSE;

	switch (surface->format)
	{
		case TPL_FORMAT_ARGB8888: wl_format = WL_DRM_FORMAT_ARGB8888; break;
		case TPL_FORMAT_XRGB8888: wl_format = WL_DRM_FORMAT_XRGB8888; break;
		case TPL_FORMAT_RGB565: wl_format = WL_DRM_FORMAT_RGB565; break;
		default: return TPL_FALSE;
	}

	name = tbm_bo_export(wayland_buffer->bo);

	wl_resource = (struct wl_resource *)wl_drm_create_buffer(wayland_display->wl_drm, (uint32_t)name,
								 buffer->width, buffer->height, buffer->pitch, wl_format);

	/* Remove from the default queue. */
	if (wl_resource)
		wl_proxy_set_queue((struct wl_proxy *)wl_resource, NULL);

	return (void *)wl_resource;
}

tpl_bool_t
__tpl_display_choose_backend_wayland(tpl_handle_t native_dpy)
{
	if (native_dpy == NULL) return TPL_FALSE;

	if (__tpl_wayland_display_is_wl_display(native_dpy)) return TPL_TRUE;
	if (__tpl_wayland_display_is_gbm_device(native_dpy)) return TPL_TRUE;

	return TPL_FALSE;
}

void
__tpl_display_init_backend_wayland(tpl_display_backend_t *backend)
{
	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init				= __tpl_wayland_display_init;
	backend->fini				= __tpl_wayland_display_fini;
	backend->query_config			= __tpl_wayland_display_query_config;
	backend->filter_config			= __tpl_wayland_display_filter_config;
	backend->get_window_info			= __tpl_wayland_display_get_window_info;
	backend->get_pixmap_info			= __tpl_wayland_display_get_pixmap_info;
	backend->flush				= __tpl_wayland_display_flush;
}

void
__tpl_surface_init_backend_wayland(tpl_surface_backend_t *backend)
{
	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init		= __tpl_wayland_surface_init;
	backend->fini		= __tpl_wayland_surface_fini;
	backend->begin_frame	= __tpl_wayland_surface_begin_frame;
	backend->end_frame	= __tpl_wayland_surface_end_frame;
	backend->validate_frame	= __tpl_wayland_surface_validate_frame;
	backend->get_buffer	= __tpl_wayland_surface_get_buffer;
	backend->post		= __tpl_wayland_surface_post;
}

void
__tpl_buffer_init_backend_wayland(tpl_buffer_backend_t *backend)
{
	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init			= __tpl_wayland_buffer_init;
	backend->fini			= __tpl_wayland_buffer_fini;
	backend->map			= __tpl_wayland_buffer_map;
	backend->unmap			= __tpl_wayland_buffer_unmap;
	backend->lock			= __tpl_wayland_buffer_lock;
	backend->unlock			= __tpl_wayland_buffer_unlock;
	backend->create_native_buffer	= __tpl_wayland_buffer_create_native_buffer;
}

/**********************************************************************************/

static void
__cb_client_wayland_drm_handle_device(void *user_data, struct wl_drm *drm, const char *device)
{
	tpl_display_t *display = (tpl_display_t *)user_data;
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;
	drm_magic_t magic;

	TPL_IGNORE(drm);

	display->bufmgr_fd = open(device, O_RDWR | O_CLOEXEC);

	drmGetMagic(display->bufmgr_fd, &magic);
	wl_drm_authenticate(wayland_display->wl_drm, magic);
}

static void
__cb_client_wayland_drm_handle_format(void *data, struct wl_drm *drm, uint32_t format)
{
	TPL_IGNORE(data);
	TPL_IGNORE(drm);
	TPL_IGNORE(format);

	return;
}

static void
__cb_client_wayland_drm_handle_authenticated(void *user_data, struct wl_drm *drm)
{
	tpl_display_t *display = (tpl_display_t *)user_data;
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;

	TPL_IGNORE(drm);

	wayland_display->proc.app.authenticated = TPL_TRUE;
}

static void
__cb_client_wayland_drm_handle_capabilities(void *data, struct wl_drm *drm, uint32_t value)
{
	TPL_IGNORE(data);
	TPL_IGNORE(drm);
	TPL_IGNORE(value);

	return;
}

static const struct wl_drm_listener wl_drm_client_listener =
{
	__cb_client_wayland_drm_handle_device,
	__cb_client_wayland_drm_handle_format,
	__cb_client_wayland_drm_handle_authenticated,
	__cb_client_wayland_drm_handle_capabilities
};

static void
__cb_client_registry_handle_global(void *user_data, struct wl_registry *registry, uint32_t name,
		       const char *interface, uint32_t version)
{
	tpl_display_t *display = (tpl_display_t *)user_data;
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;

	if (strcmp(interface, "wl_drm") == 0)
	{
		wayland_display->wl_drm = wl_registry_bind(registry, name, &wl_drm_interface, (version > 2) ? 2 : version);
		wl_drm_add_listener(wayland_display->wl_drm, &wl_drm_client_listener, display);
	}
}

static const struct wl_registry_listener registry_listener =
{
	__cb_client_registry_handle_global,
	NULL
};

static void
__cb_client_sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
   int *done = data;

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
__cb_client_buffer_release_callback(void *data, struct wl_resource *resource)
{
	tpl_buffer_t *buffer = (tpl_buffer_t *)data;
	tpl_surface_t *surface = buffer->surface;

	TPL_LOG(3, "release window(%p, %p), buffer(%p), key:%d",
            surface, surface?surface->native_handle:NULL,
            buffer, buffer->key);

	if (surface != NULL)
	{
		TPL_OBJECT_LOCK(surface);

		/* MOVE BUFFER : [done queue] --> [able queue] */
		{
			tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;
			tpl_list_node_t *node = tpl_list_get_front_node(&wayland_surface->done_rendering_queue);

			while (node != NULL)
			{
				tpl_buffer_t *node_buffer = (tpl_buffer_t *)tpl_list_node_get_data(node);

				if (node_buffer == buffer)
				{
					tpl_wayland_buffer_t *wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;

					if (wayland_buffer->proc.app.resized)
					{
						/* Delete the buffer that had been created before resized. */
						__tpl_wayland_surface_buffer_free(buffer);

						tpl_list_push_back(&wayland_surface->able_rendering_queue, NULL);
					}
					else
					{
						/* Reuse the buffer because the buffer size is not changed and reusable. */
						tpl_list_push_back(&wayland_surface->able_rendering_queue, buffer);
					}

					tpl_list_remove(node, NULL);
					break;
				}

				node = tpl_list_node_next(node);
			}
			TPL_ASSERT(node != NULL);
		}
		TPL_OBJECT_UNLOCK(surface);
	}

	tpl_object_unreference((tpl_object_t *)buffer);
}

static const struct wl_buffer_listener buffer_release_listener = {
	(void *)__cb_client_buffer_release_callback,
};

/**********************************************************************************/

static struct gbm_bo *
__cb_server_gbm_surface_lock_front_buffer(struct gbm_surface *gbm_surf)
{
	struct gbm_tbm_surface *gbm_tbm_surf = (struct gbm_tbm_surface *)gbm_surf;
	tpl_surface_t *surface = (tpl_surface_t *)gbm_tbm_surface_get_user_data(gbm_tbm_surf);
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;

	TPL_ASSERT(surface != NULL);

	TPL_OBJECT_LOCK(surface);

	wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;

	while (1)
	{
		/* Wait for posted to prevent locking not-rendered buffer. */
		if (!tpl_list_is_empty(&wayland_surface->done_rendering_queue))
		{
			buffer = tpl_list_get_front(&wayland_surface->done_rendering_queue);
			wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;
			if (wayland_buffer->proc.comp.posted)
				break;
		}
		TPL_OBJECT_UNLOCK(surface);
		tpl_util_sys_yield();
		TPL_OBJECT_LOCK(surface);
	}

	TPL_ASSERT(buffer != NULL);

	wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;

	tbm_bo_map(wayland_buffer->bo, TBM_DEVICE_MM, TBM_OPTION_READ | TBM_OPTION_WRITE);

	TPL_OBJECT_UNLOCK(surface);

	return wayland_buffer->proc.comp.gbm_bo;
}

static void
__cb_server_gbm_surface_release_buffer(struct gbm_surface *gbm_surf, struct gbm_bo *gbm_bo)
{
	struct gbm_tbm_bo *gbm_tbm_bo = (struct gbm_tbm_bo *)gbm_bo;
	tbm_bo bo;

	TPL_IGNORE(gbm_surf);

	bo = gbm_tbm_bo_get_tbm_bo(gbm_tbm_bo);
	TPL_ASSERT(bo);

	tbm_bo_unmap(bo);
}

static int
__cb_server_gbm_surface_has_free_buffers(struct gbm_surface *gbm_surf)
{
	struct gbm_tbm_surface *gbm_tbm_surf = (struct gbm_tbm_surface *)gbm_surf;
	tpl_surface_t *surface = (tpl_surface_t *)gbm_tbm_surface_get_user_data(gbm_tbm_surf);
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_buffer_t *buffer = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;

	TPL_ASSERT(surface != NULL);

	TPL_OBJECT_LOCK(surface);

	wayland_surface = (tpl_wayland_surface_t *)surface->backend.data;

	if (tpl_list_is_empty(&wayland_surface->done_rendering_queue)) {
		TPL_OBJECT_UNLOCK(surface);
		return 0;
	}

	buffer = tpl_list_get_front(&wayland_surface->done_rendering_queue);
	wayland_buffer = (tpl_wayland_buffer_t *)buffer->backend.data;

	if (wayland_buffer->proc.comp.posted) {
		TPL_OBJECT_UNLOCK(surface);
		return 1;
	}

	TPL_OBJECT_UNLOCK(surface);

	return 0;
}

#ifdef EGL_BIND_WL_DISPLAY
static struct wayland_drm_callbacks wl_drm_server_listener;

static int
__cb_server_wayland_drm_display_authenticate(void *user_data, uint32_t magic)
{
	tpl_display_t *display = (tpl_display_t *)user_data;

	return drmAuthMagic(display->bufmgr_fd, magic);
}

static void
__cb_server_wayland_drm_reference_buffer(void *user_data, uint32_t name, int fd, struct wl_drm_buffer *buffer)
{
	tpl_display_t *display = (tpl_display_t *)user_data;
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;

	TPL_IGNORE(name);
	TPL_IGNORE(fd);

	buffer->driver_buffer = tbm_bo_import(wayland_display->bufmgr, name);
}

static void
__cb_server_wayland_drm_unreference_buffer(void *user_data, struct wl_drm_buffer *buffer)
{
	tpl_display_t *display = (tpl_display_t *)user_data;
	tpl_wayland_display_t *wayland_display = (tpl_wayland_display_t *)display->backend.data;

	tbm_bo_unref(buffer->driver_buffer);
	buffer->driver_buffer = NULL;

	/* TODO: tpl_buffer is NULL, it's not right */
	__tpl_wayland_surface_buffer_cache_remove(&wayland_display->proc.comp.cached_buffers, (unsigned int)buffer);
}

static struct wayland_drm_callbacks wl_drm_server_listener =
{
	__cb_server_wayland_drm_display_authenticate,
	__cb_server_wayland_drm_reference_buffer,
	__cb_server_wayland_drm_unreference_buffer
};

unsigned int __egl_platform_bind_wayland_display(void * display, struct wl_display *wayland_display)
{
	tpl_display_t *tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, display);
	tpl_wayland_display_t *tpl_wayland_display = (tpl_wayland_display_t *)tpl_display->backend.data;
	char *device_name = NULL;

	tpl_display->bufmgr_fd = dup(gbm_device_get_fd(tpl_display->native_handle));
	tpl_wayland_display->bufmgr = tbm_bufmgr_init(tpl_display->bufmgr_fd);

	device_name = drmGetDeviceNameFromFd(tpl_display->bufmgr_fd);
	tpl_wayland_display->wl_drm = wayland_drm_init((struct wl_display *)wayland_display, device_name, &wl_drm_server_listener, tpl_display, 0);

	return TPL_TRUE;
}

unsigned int __egl_platform_unbind_wayland_display(void * display, struct wl_display *wayland_display)
{
	tpl_display_t *tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, display);
	tpl_wayland_display_t *tpl_wayland_display = (tpl_wayland_display_t *)tpl_display->backend.data;

	if (tpl_wayland_display->wl_drm == NULL)
		return TPL_FALSE;

	wayland_drm_uninit(tpl_wayland_display->wl_drm);
	tbm_bufmgr_deinit(tpl_wayland_display->bufmgr);
	close(tpl_display->bufmgr_fd);

	return TPL_TRUE;
}
#endif
