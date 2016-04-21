#include "tpl_internal.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tbm_surface_queue.h>

typedef struct _tpl_tbm_display tpl_tbm_display_t;
typedef struct _tpl_tbm_surface tpl_tbm_surface_t;

struct _tpl_tbm_display {
	int dummy;
};

struct _tpl_tbm_surface {
	int dummy;
};

static tpl_result_t
__tpl_tbm_display_init(tpl_display_t *display)
{
	tpl_tbm_display_t *tbm_display = NULL;

	TPL_ASSERT(display);

	if (!display->native_handle) {
		display->native_handle = tbm_bufmgr_init(-1);
	}

	tbm_display = (tpl_tbm_display_t *) calloc(1, sizeof(tpl_tbm_display_t));

	if (!tbm_display) {
		TPL_ERR("Failed to allocate memory for new tpl_tbm_display_t.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	display->backend.data = tbm_display;
	display->bufmgr_fd = -1;

	return TPL_ERROR_NONE;
}

static void
__tpl_tbm_display_fini(tpl_display_t *display)
{
	tpl_tbm_display_t *tbm_display;

	TPL_ASSERT(display);

	tbm_display = (tpl_tbm_display_t *)display->backend.data;

	if (tbm_display) free(tbm_display);

	display->backend.data = NULL;

	tbm_bufmgr_deinit((tbm_bufmgr)display->native_handle);
}

static tpl_result_t
__tpl_tbm_display_query_config(tpl_display_t *display,
			       tpl_surface_type_t surface_type, int red_size,
			       int green_size, int blue_size, int alpha_size,
			       int color_depth, int *native_visual_id,
			       tpl_bool_t *is_slow)
{
	TPL_ASSERT(display);

	if (surface_type == TPL_SURFACE_TYPE_WINDOW && red_size == 8
	    && green_size == 8 && blue_size == 8
	    && (color_depth == 32 || color_depth == 24)) {
		if (alpha_size == 8) {
			if (native_visual_id)
				*native_visual_id = TBM_FORMAT_ARGB8888;

			if (is_slow) *is_slow = TPL_FALSE;

			return TPL_ERROR_NONE;
		}
		if (alpha_size == 0) {
			if (native_visual_id)
				*native_visual_id = TBM_FORMAT_XRGB8888;

			if (is_slow) *is_slow = TPL_FALSE;

			return TPL_ERROR_NONE;
		}
	}

	return TPL_ERROR_INVALID_PARAMETER;
}

static tpl_result_t
__tpl_tbm_display_filter_config(tpl_display_t *display, int *visual_id,
				int alpha_size)
{
	TPL_IGNORE(display);

	if (visual_id && *visual_id == TBM_FORMAT_ARGB8888 && alpha_size == 0) {
		*visual_id = TBM_FORMAT_XRGB8888;
		return TPL_ERROR_NONE;
	}

	return TPL_ERROR_INVALID_PARAMETER;
}

static tpl_result_t
__tpl_tbm_display_get_window_info(tpl_display_t *display, tpl_handle_t window,
				  int *width, int *height, tbm_format *format,
				  int depth, int a_size)
{
	TPL_ASSERT(display);
	TPL_ASSERT(window);

	tbm_surface_queue_h surf_queue = (tbm_surface_queue_h)window;
	if (!surf_queue) {
		TPL_ERR("Native widow(%p) is invalid.", window);
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (width) *width = tbm_surface_queue_get_width(surf_queue);
	if (height) *height = tbm_surface_queue_get_height(surf_queue);
	if (format) *format = tbm_surface_queue_get_format(surf_queue);

	return TPL_ERROR_NONE;
}

static tpl_result_t
__tpl_tbm_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
				  int *width, int *height, tbm_format *format)
{
	tbm_surface_h	tbm_surface = NULL;

	tbm_surface = (tbm_surface_h)pixmap;
	if (!tbm_surface) {
		TPL_ERR("Native pixmap(%p) is invalid.", pixmap);
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (width) *width = tbm_surface_get_width(tbm_surface);
	if (height) *height = tbm_surface_get_height(tbm_surface);
	if (format) *format = tbm_surface_get_format(tbm_surface);

	return TPL_ERROR_NONE;
}

static tbm_surface_h
__tpl_tbm_display_get_buffer_from_native_pixmap(tpl_handle_t pixmap)
{
	TPL_ASSERT(pixmap);
	return (tbm_surface_h)pixmap;
}

static void
__tpl_tbm_surface_queue_notify_cb(tbm_surface_queue_h surface_queue, void *data)
{
	/* Do something */
}

static tpl_result_t
__tpl_tbm_surface_init(tpl_surface_t *surface)
{
	tpl_tbm_surface_t *tpl_tbm_surface = NULL;
	TPL_ASSERT(surface);

	tpl_tbm_surface = (tpl_tbm_surface_t *) calloc(1, sizeof(tpl_tbm_surface_t));
	if (!tpl_tbm_surface) {
		TPL_ERR("Failed to allocate memory for new tpl_tbm_surface_t");
		return TPL_ERROR_INVALID_OPERATION;
	}

	surface->backend.data = (void *)tpl_tbm_surface;

	if (surface->type == TPL_SURFACE_TYPE_WINDOW) {
		if (__tpl_tbm_display_get_window_info(surface->display,
						      surface->native_handle, &surface->width,
						      &surface->height, NULL, 0, 0) != TPL_ERROR_NONE) {
			TPL_ERR("Failed to get native window(%p) info.",
				surface->native_handle);
			goto error;
		}

		tbm_surface_queue_add_destroy_cb((tbm_surface_queue_h)surface->native_handle,
						 __tpl_tbm_surface_queue_notify_cb,
						 surface);

		TPL_LOG(3, "window(%p, %p) %dx%d", surface,
			surface->native_handle, surface->width, surface->height);

		return TPL_ERROR_NONE;
	} else if (surface->type == TPL_SURFACE_TYPE_PIXMAP) {
		if (__tpl_tbm_display_get_pixmap_info(surface->display,
						      surface->native_handle, &surface->width,
						      &surface->height, NULL) != TPL_TRUE) {
			TPL_ERR("Failed to get native pixmap(%p) info.",
				surface->native_handle);

			goto error;
		}

		tbm_surface_internal_ref((tbm_surface_h)surface->native_handle);

		TPL_LOG(3, "pixmap(%p, %p) %dx%d", surface,
			surface->native_handle, surface->width, surface->height);
		return TPL_ERROR_NONE;
	}

error:
	free(tpl_tbm_surface);
	surface->backend.data = NULL;

	return TPL_ERROR_INVALID_OPERATION;
}

static void
__tpl_tbm_surface_fini(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	TPL_LOG(3, "%s(%p, %p)",
		((surface->type == TPL_SURFACE_TYPE_WINDOW) ? "window" : "pixmap"),
		surface, surface->native_handle);

	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
		tbm_surface_internal_unref((tbm_surface_h)surface->native_handle);

	if (surface->type == TPL_SURFACE_TYPE_WINDOW) {
		/*TODO: we need fix for dequeued surface*/
	}

	free(surface->backend.data);
	surface->backend.data = NULL;
}

static tpl_result_t
__tpl_tbm_surface_enqueue_buffer(tpl_surface_t *surface,
				 tbm_surface_h tbm_surface, int num_rects,
				 const int *rects)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);
	TPL_ASSERT(tbm_surface);
	TPL_IGNORE(num_rects);
	TPL_IGNORE(rects);

	tbm_surface_internal_unref(tbm_surface);

	if (surface->type == TPL_SURFACE_TYPE_PIXMAP) {
		TPL_ERR("Pixmap cannot post(%p, %p)", surface,
			surface->native_handle);
		return TPL_ERROR_INVALID_PARAMETER;
	}

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	tbm_surface_queue_h tbm_queue = (tbm_surface_queue_h)surface->native_handle;

	if (!tbm_queue) {
		TPL_ERR("tbm_surface_queue is invalid.");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (tbm_surface_queue_enqueue(tbm_queue, tbm_surface)
	    != TBM_SURFACE_QUEUE_ERROR_NONE) {
		TPL_ERR("tbm_surface_queue_enqueue failed. tbm_queue(%p) tbm_surface(%p)",
			tbm_queue, tbm_surface);
		return TPL_ERROR_INVALID_OPERATION;
	}

	return TPL_ERROR_NONE;
}

static tpl_bool_t
__tpl_tbm_surface_validate(tpl_surface_t *surface)
{
	TPL_IGNORE(surface);

	return TPL_TRUE;
}

static tbm_surface_h
__tpl_tbm_surface_dequeue_buffer(tpl_surface_t *surface)
{
	tbm_surface_h tbm_surface = NULL;
	tbm_surface_queue_h tbm_queue = NULL;
	tbm_surface_queue_error_e tsq_err = 0;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->native_handle);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);

	tbm_queue = (tbm_surface_queue_h)surface->native_handle;

	tsq_err = tbm_surface_queue_dequeue(tbm_queue, &tbm_surface);
	if (!tbm_surface  && tbm_surface_queue_can_dequeue(tbm_queue, 1) == 1) {
		tsq_err = tbm_surface_queue_dequeue(tbm_queue, &tbm_surface);
		if (!tbm_surface) {
			TPL_ERR("Failed to get tbm_surface from tbm_surface_queue | tsq_err = %d",
				tsq_err);
			return NULL;
		}
	}

	/* Inc ref count about tbm_surface */
	/* It will be dec when before tbm_surface_queue_enqueue called */
	tbm_surface_internal_ref(tbm_surface);

	return tbm_surface;
}

tpl_bool_t
__tpl_display_choose_backend_tbm(tpl_handle_t native_dpy)
{
	tpl_bool_t ret = TPL_FALSE;
	tbm_bufmgr bufmgr = NULL;

	if (!native_dpy) return TPL_FALSE;

	bufmgr = tbm_bufmgr_init(-1);

	if (bufmgr == (tbm_bufmgr)native_dpy) ret = TPL_TRUE;

	if (bufmgr) tbm_bufmgr_deinit(bufmgr);

	return ret;
}

void
__tpl_display_init_backend_tbm(tpl_display_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_TBM;
	backend->data = NULL;

	backend->init = __tpl_tbm_display_init;
	backend->fini = __tpl_tbm_display_fini;
	backend->query_config = __tpl_tbm_display_query_config;
	backend->filter_config = __tpl_tbm_display_filter_config;
	backend->get_window_info = __tpl_tbm_display_get_window_info;
	backend->get_pixmap_info = __tpl_tbm_display_get_pixmap_info;
	backend->get_buffer_from_native_pixmap =
		__tpl_tbm_display_get_buffer_from_native_pixmap;
}

void
__tpl_surface_init_backend_tbm(tpl_surface_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_TBM;
	backend->data = NULL;

	backend->init = __tpl_tbm_surface_init;
	backend->fini = __tpl_tbm_surface_fini;
	backend->validate = __tpl_tbm_surface_validate;
	backend->dequeue_buffer = __tpl_tbm_surface_dequeue_buffer;
	backend->enqueue_buffer = __tpl_tbm_surface_enqueue_buffer;
}

