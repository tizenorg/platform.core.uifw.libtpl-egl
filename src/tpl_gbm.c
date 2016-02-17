#define inline __inline__

#include <wayland-client.h>

#include <drm.h>

#ifndef USE_TBM_QUEUE
#define USE_TBM_QUEUE
#endif

#include <gbm.h>
#include <gbm/gbm_tbm.h>
#include <gbm/gbm_tbmint.h>
#include <xf86drm.h>

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

struct _tpl_gbm_display {
	tbm_bufmgr bufmgr;
};

struct _tpl_gbm_surface {
	tbm_surface_queue_h tbm_queue;
	tbm_surface_h current_buffer;
};

struct _tpl_gbm_buffer {
	tpl_display_t *display;
	tpl_gbm_buffer_t **tpl_gbm_surface;
	tbm_bo bo;
	struct gbm_bo *gbm_bo;
	struct wl_listener destroy_listener;
};

static int tpl_gbm_buffer_key;
#define KEY_TPL_GBM_BUFFER  (unsigned long)(&tpl_gbm_buffer_key)

static void __tpl_gbm_buffer_free(tpl_gbm_buffer_t *gbm_buffer);
static inline tpl_gbm_buffer_t *
__tpl_gbm_get_gbm_buffer_from_tbm_surface(tbm_surface_h surface)
{
	tbm_bo bo;
	tpl_gbm_buffer_t *buf=NULL;

	bo = tbm_surface_internal_get_bo(surface, 0);
	tbm_bo_get_user_data(bo, KEY_TPL_GBM_BUFFER, (void **)&buf);

	return buf;
}

static inline void
__tpl_gbm_set_gbm_buffer_to_tbm_surface(tbm_surface_h surface,
					tpl_gbm_buffer_t *buf)
{
	tbm_bo bo;

	bo = tbm_surface_internal_get_bo(surface, 0);
	tbm_bo_add_user_data(bo, KEY_TPL_GBM_BUFFER,
		(tbm_data_free)__tpl_gbm_buffer_free);
	tbm_bo_set_user_data(bo, KEY_TPL_GBM_BUFFER, buf);
}

static TPL_INLINE tpl_bool_t
__tpl_gbm_display_is_gbm_device(tpl_handle_t native_dpy)
{
	TPL_ASSERT(native_dpy);

	/* MAGIC CHECK: A native display handle is a gbm_device if the de-referenced first value
	   is a memory address pointing gbm_create_surface(). */
	if (*(void **)native_dpy == gbm_create_device)
		return TPL_TRUE;

	return TPL_FALSE;
}

static tpl_result_t
__tpl_gbm_display_init(tpl_display_t *display)
{
	tpl_gbm_display_t *gbm_display = NULL;

	TPL_ASSERT(display);

	/* Do not allow default display in gbm. */
	if (!display->native_handle) return TPL_ERROR_INVALID_PARAMETER;

	gbm_display = (tpl_gbm_display_t *) calloc(1, sizeof(tpl_gbm_display_t));
	if (!gbm_display) return TPL_ERROR_INVALID_PARAMETER;

	display->bufmgr_fd = dup(gbm_device_get_fd(display->native_handle));
	gbm_display->bufmgr = tbm_bufmgr_init(display->bufmgr_fd);
	display->backend.data = gbm_display;

	return TPL_ERROR_NONE;
}

static void
__tpl_gbm_display_fini(tpl_display_t *display)
{
	tpl_gbm_display_t *gbm_display;

	TPL_ASSERT(display);

	gbm_display = (tpl_gbm_display_t *)display->backend.data;
	if (gbm_display) {
		tbm_bufmgr_deinit(gbm_display->bufmgr);
		free(gbm_display);
	}
	close(display->bufmgr_fd);
	display->backend.data = NULL;
}

static tpl_result_t
__tpl_gbm_display_query_config(tpl_display_t *display,
			       tpl_surface_type_t surface_type, int red_size,
			       int green_size, int blue_size, int alpha_size,
			       int color_depth, int *native_visual_id,
			       tpl_bool_t *is_slow)
{
	TPL_ASSERT(display);

	if ((surface_type == TPL_SURFACE_TYPE_WINDOW) && (red_size == 8)
		&& (green_size == 8) && (blue_size == 8)
		&& ((color_depth == 32) || (color_depth == 24))) {
		if (alpha_size == 8) {
			if (gbm_device_is_format_supported(
				(struct gbm_device *)display->native_handle,
				GBM_FORMAT_ARGB8888, GBM_BO_USE_RENDERING) == 1) {
				if (native_visual_id)
					*native_visual_id = GBM_FORMAT_ARGB8888;
			} else return TPL_ERROR_INVALID_PARAMETER;

			if (is_slow != NULL) *is_slow = TPL_FALSE;

			return TPL_ERROR_NONE;
		}
		if (alpha_size == 0) {
			if (gbm_device_is_format_supported(
				(struct gbm_device *)display->native_handle,
				GBM_FORMAT_XRGB8888,
				GBM_BO_USE_RENDERING) == 1) {
				if (native_visual_id)
					*native_visual_id = GBM_FORMAT_XRGB8888;
			} else return TPL_ERROR_INVALID_PARAMETER;

			if (is_slow != NULL) *is_slow = TPL_FALSE;

			return TPL_ERROR_NONE;
		}
	}

	return TPL_ERROR_INVALID_PARAMETER;
}

static tpl_result_t
__tpl_gbm_display_filter_config(tpl_display_t *display, int *visual_id,
				int alpha_size)
{
	TPL_IGNORE(display);

	if (visual_id != NULL && *visual_id == GBM_FORMAT_ARGB8888
		&& alpha_size == 0) {
		*visual_id = GBM_FORMAT_XRGB8888;
		return TPL_ERROR_NONE;
	}

	return TPL_ERROR_INVALID_PARAMETER;
}

static tpl_result_t
__tpl_gbm_display_get_window_info(tpl_display_t *display, tpl_handle_t window,
				  int *width, int *height, tbm_format *format,
				  int depth, int a_size)
{
	TPL_ASSERT(display);
	TPL_ASSERT(window);

	struct gbm_surface *gbm_surface = (struct gbm_surface *)window;
	tbm_surface_queue_h surf_queue = (tbm_surface_queue_h)gbm_tbm_get_surface_queue(gbm_surface);
	if (!surf_queue) {
		TPL_ERR("Failed to get tbm_surface_queue from gbm_surface.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	if (width) *width = tbm_surface_queue_get_width(surf_queue);
	if (height) *height = tbm_surface_queue_get_height(surf_queue);
	if (format) *format = tbm_surface_queue_get_format(surf_queue);

	return TPL_ERROR_NONE;
}

static tpl_result_t
__tpl_gbm_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
				  int *width, int *height, tbm_format *format)
{
	tbm_surface_h	tbm_surface = NULL;

	tbm_surface = wayland_tbm_server_get_surface(NULL, (struct wl_resource*)pixmap);
	if (!tbm_surface) {
		TPL_ERR("Failed to get tbm_surface_h from native pixmap.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	if (width) *width = tbm_surface_get_width(tbm_surface);
	if (height) *height = tbm_surface_get_height(tbm_surface);
	if (format) *format = tbm_surface_get_format(tbm_surface);

	return TPL_ERROR_NONE;
}

static tbm_surface_h
__tpl_gbm_display_get_buffer_from_native_pixmap(tpl_handle_t pixmap)
{
	tbm_surface_h tbm_surface = NULL;

	TPL_ASSERT(pixmap);

	tbm_surface = wayland_tbm_server_get_surface(NULL, (struct wl_resource*)pixmap);
	if (!tbm_surface) {
		TPL_ERR("Failed to get tbm_surface_h from wayland_tbm.");
		return NULL;
	}

	return tbm_surface;
}

static tpl_result_t
__tpl_gbm_surface_init(tpl_surface_t *surface)
{
	tpl_gbm_surface_t *tpl_gbm_surface = NULL;
	TPL_ASSERT(surface);

	tpl_gbm_surface = (tpl_gbm_surface_t *) calloc(1, sizeof(tpl_gbm_surface_t));
	if (!tpl_gbm_surface) {
		TPL_ERR("Failed to allocate new gbm backend surface.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	surface->backend.data = (void *)tpl_gbm_surface;
	tpl_gbm_surface->tbm_queue = NULL;
	tpl_gbm_surface->current_buffer = NULL;

	if (surface->type == TPL_SURFACE_TYPE_WINDOW) {
		struct gbm_surface *gbm_surface = (struct gbm_surface*)surface->native_handle;
		tpl_gbm_surface->tbm_queue = (tbm_surface_queue_h)gbm_tbm_get_surface_queue(gbm_surface);
		if (!tpl_gbm_surface->tbm_queue) {
			TPL_ERR("Failed to get tbm_surface_queue from gbm_surface.");
			goto error;
		}

		if (__tpl_gbm_display_get_window_info(surface->display,
			surface->native_handle, &surface->width,
			&surface->height, NULL, 0, 0) != TPL_ERROR_NONE) {
			TPL_ERR("Failed to get native window info.");
			goto error;
		}

		TPL_LOG(3, "window(%p, %p) %dx%d", surface, surface->native_handle, surface->width, surface->height);
		return TPL_ERROR_NONE;
	} else if (surface->type == TPL_SURFACE_TYPE_PIXMAP) {
		if (__tpl_gbm_display_get_pixmap_info(surface->display,
			surface->native_handle, &surface->width,
			&surface->height, NULL) != TPL_ERROR_NONE) {
			TPL_ERR("Failed to get native pixmap info.");
			goto error;
		}

		return TPL_ERROR_NONE;
	}

error:
	free(tpl_gbm_surface);

	return TPL_ERROR_INVALID_OPERATION;
}

static void
__tpl_gbm_surface_fini(tpl_surface_t *surface)
{
	tpl_gbm_surface_t *gbm_surface = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	gbm_surface = (tpl_gbm_surface_t *) surface->backend.data;
	if (!gbm_surface) return;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (gbm_surface->current_buffer)
		tbm_surface_internal_unref(gbm_surface->current_buffer);

	free(gbm_surface);
	surface->backend.data = NULL;
}

static tpl_result_t
__tpl_gbm_surface_enqueue_buffer(tpl_surface_t *surface,
				 tbm_surface_h tbm_surface, int num_rects,
				 const int *rects)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);
	TPL_ASSERT(tbm_surface);
	TPL_IGNORE(num_rects);
	TPL_IGNORE(rects);

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	tpl_gbm_surface_t *gbm_surface = (tpl_gbm_surface_t*)surface->backend.data;
	if (!gbm_surface) {
		TPL_ERR("tpl_gbm_surface_t is invalid. tpl_surface_t(%p)",
			surface);
		return TPL_ERROR_INVALID_PARAMETER;
	}

	tbm_surface_internal_unref(tbm_surface);

	if (!gbm_surface->tbm_queue) {
		TPL_ERR("tbm_surface_queue is invalid. tpl_gbm_surface_t(%p)",
			gbm_surface);
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (tbm_surface_queue_enqueue(gbm_surface->tbm_queue, tbm_surface)
		!= TBM_SURFACE_QUEUE_ERROR_NONE) {
		TPL_ERR("tbm_surface_queue_enqueue failed. tbm_surface_queue(%p) tbm_surface(%p)",
			gbm_surface->tbm_queue, tbm_surface);
		return TPL_ERROR_INVALID_PARAMETER;
	}

	return TPL_ERROR_NONE;
}

static tpl_bool_t
__tpl_gbm_surface_validate(tpl_surface_t *surface)
{
	TPL_IGNORE(surface);

	return TPL_TRUE;
}

static tbm_surface_h
__tpl_gbm_surface_dequeue_buffer(tpl_surface_t *surface)
{
	tbm_bo bo;
	tbm_surface_h tbm_surface = NULL;
	tbm_surface_queue_error_e tsq_err = 0;
	tpl_gbm_buffer_t *gbm_buffer = NULL;

	tpl_gbm_surface_t *gbm_surface = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->native_handle);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);

	gbm_surface = (tpl_gbm_surface_t*)surface->backend.data;

	tsq_err = tbm_surface_queue_dequeue(gbm_surface->tbm_queue, &tbm_surface);
	if(!tbm_surface
		&& tbm_surface_queue_can_dequeue(gbm_surface->tbm_queue, 1) == 1) {
		tsq_err = tbm_surface_queue_dequeue(gbm_surface->tbm_queue, &tbm_surface);
		if (!tbm_surface) {
			TPL_ERR("Failed to get tbm_surface from tbm_surface_queue | tsq_err = %d",
				tsq_err);
			return NULL;
		}
	}
	/* Inc ref count about tbm_surface */
	/* It will be dec when before tbm_surface_queue_enqueue called */
	tbm_surface_internal_ref(tbm_surface);

	if (gbm_buffer = __tpl_gbm_get_gbm_buffer_from_tbm_surface(tbm_surface)) {
		return tbm_surface;
	}

	if (!(bo = tbm_surface_internal_get_bo(tbm_surface, 0))) {
		TPL_ERR("Failed to get tbm_bo from tbm_surface");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	gbm_buffer = (tpl_gbm_buffer_t *) calloc(1, sizeof(tpl_gbm_buffer_t));
	if (!gbm_buffer) {
		TPL_ERR("Mem alloc for gbm_buffer failed!");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	gbm_buffer->display = surface->display;
	gbm_buffer->bo = bo;

	gbm_surface->current_buffer = tbm_surface;

	__tpl_gbm_set_gbm_buffer_to_tbm_surface(tbm_surface, gbm_buffer);

	return tbm_surface;
}

static void
__tpl_gbm_buffer_free(tpl_gbm_buffer_t *gbm_buffer)
{
	TPL_ASSERT(gbm_buffer);
	free(gbm_buffer);
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

	backend->init = __tpl_gbm_display_init;
	backend->fini = __tpl_gbm_display_fini;
	backend->query_config = __tpl_gbm_display_query_config;
	backend->filter_config = __tpl_gbm_display_filter_config;
	backend->get_window_info = __tpl_gbm_display_get_window_info;
	backend->get_pixmap_info = __tpl_gbm_display_get_pixmap_info;
	backend->get_buffer_from_native_pixmap = __tpl_gbm_display_get_buffer_from_native_pixmap;
}

void
__tpl_surface_init_backend_gbm(tpl_surface_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_GBM;
	backend->data = NULL;

	backend->init = __tpl_gbm_surface_init;
	backend->fini = __tpl_gbm_surface_fini;
	backend->validate = __tpl_gbm_surface_validate;
	backend->dequeue_buffer = __tpl_gbm_surface_dequeue_buffer;
	backend->enqueue_buffer = __tpl_gbm_surface_enqueue_buffer;
}
