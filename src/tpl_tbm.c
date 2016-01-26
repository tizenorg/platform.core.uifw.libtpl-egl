#include "tpl_internal.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tbm_surface_queue.h>

typedef struct _tpl_tbm_display		tpl_tbm_display_t;
typedef struct _tpl_tbm_surface		tpl_tbm_surface_t;

struct _tpl_tbm_display
{
	int dummy;
};

struct _tpl_tbm_surface
{
	int dummy;
};

static inline tpl_format_t
__tpl_tbm_get_tpl_format(tbm_format format)
{
	tpl_format_t ret;
	switch (format)
	{
		case TBM_FORMAT_ARGB8888: ret = TPL_FORMAT_ARGB8888; break;
		case TBM_FORMAT_XRGB8888: ret = TPL_FORMAT_XRGB8888; break;
		case TBM_FORMAT_RGB565: ret = TPL_FORMAT_RGB565; break;
		default: ret = TPL_FORMAT_INVALID; break;
	}

	return ret;
}

static tpl_bool_t
__tpl_tbm_display_init(tpl_display_t *display)
{
	tpl_tbm_display_t *tbm_display = NULL;

	TPL_ASSERT(display);

	if (display->native_handle == NULL)
	{
		display->native_handle = tbm_bufmgr_init(-1);
	}

	tbm_display = (tpl_tbm_display_t *) calloc(1, sizeof(tpl_tbm_display_t));
	if (tbm_display == NULL)
		return TPL_FALSE;

	display->backend.data = tbm_display;
	display->bufmgr_fd = -1;

	return TPL_TRUE;
}

static void
__tpl_tbm_display_fini(tpl_display_t *display)
{
	tpl_tbm_display_t *tbm_display;

	TPL_ASSERT(display);

	tbm_display = (tpl_tbm_display_t *)display->backend.data;
	if (tbm_display != NULL)
	{
		free(tbm_display);
	}
	display->backend.data = NULL;

	tbm_bufmgr_deinit((tbm_bufmgr)display->native_handle);
}

static tpl_bool_t
__tpl_tbm_display_query_config(tpl_display_t *display, tpl_surface_type_t surface_type,
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
			if (native_visual_id != NULL)
				*native_visual_id = TBM_FORMAT_ARGB8888;

			if (is_slow != NULL)
				*is_slow = TPL_FALSE;

			return TPL_TRUE;
		}
		if (alpha_size == 0)
		{
			if (native_visual_id != NULL)
				*native_visual_id = TBM_FORMAT_XRGB8888;

			if (is_slow != NULL)
				*is_slow = TPL_FALSE;

			return TPL_TRUE;
		}
	}

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_tbm_display_filter_config(tpl_display_t *display,
				   int *visual_id, int alpha_size)
{
	TPL_IGNORE(display);

	if (visual_id != NULL && *visual_id == TBM_FORMAT_ARGB8888 && alpha_size == 0)
	{
		*visual_id = TBM_FORMAT_XRGB8888;
		return TPL_TRUE;
	}

	return TPL_FALSE;
}

static tpl_bool_t
__tpl_tbm_display_get_window_info(tpl_display_t *display, tpl_handle_t window,
					  int *width, int *height, tpl_format_t *format, int depth, int a_size)
{
	TPL_ASSERT(display);
	TPL_ASSERT(window);

	tbm_surface_queue_h surf_queue = (tbm_surface_queue_h)window;

	if (width != NULL)
		*width = tbm_surface_queue_get_width(surf_queue);
	if (height != NULL)
		*height = tbm_surface_queue_get_height(surf_queue);
	if (format != NULL)
		*format = __tpl_tbm_get_tpl_format(tbm_surface_queue_get_format(surf_queue));

	return TPL_TRUE;
}

static tpl_bool_t
__tpl_tbm_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
					  int *width, int *height, tpl_format_t *format)
{
	tbm_surface_h	tbm_surface = NULL;

	tbm_surface = (tbm_surface_h)pixmap;
	if (tbm_surface == NULL)
		return TPL_FALSE;

	if (width)
		*width = tbm_surface_get_width(tbm_surface);
	if (height)
		*height = tbm_surface_get_height(tbm_surface);
	if (format)
		*format = __tpl_tbm_get_tpl_format(tbm_surface_get_format(tbm_surface));

	return TPL_TRUE;
}

static void
__tpl_tbm_display_flush(tpl_display_t *display)
{
	TPL_IGNORE(display);

	/* Do nothing. */
}

static void
__tpl_tbm_surface_queue_notify_cb(tbm_surface_queue_h surface_queue, void *data)
{
	/* Do something */
}

static tpl_bool_t
__tpl_tbm_surface_init(tpl_surface_t *surface)
{
	tpl_tbm_surface_t *tpl_tbm_surface = NULL;
	TPL_ASSERT(surface);

	tpl_tbm_surface = (tpl_tbm_surface_t *) calloc(1, sizeof(tpl_tbm_surface_t));
	if (NULL == tpl_tbm_surface)
		return TPL_FALSE;

	surface->backend.data = (void *)tpl_tbm_surface;

	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		if (TPL_TRUE != __tpl_tbm_display_get_window_info(surface->display, surface->native_handle,
					&surface->width, &surface->height, NULL, 0, 0))
			goto error;

		tbm_surface_queue_set_destroy_cb((tbm_surface_queue_h)surface->native_handle,
							__tpl_tbm_surface_queue_notify_cb, surface);

		TPL_LOG(3, "window(%p, %p) %dx%d", surface, surface->native_handle, surface->width, surface->height);
		return TPL_TRUE;
	}
	else if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		if (TPL_TRUE != __tpl_tbm_display_get_pixmap_info(surface->display, surface->native_handle,
					&surface->width, &surface->height, NULL))
			goto error;

		tbm_surface_internal_ref((tbm_surface_h)surface->native_handle);

		TPL_LOG(3, "pixmap(%p, %p) %dx%d", surface, surface->native_handle, surface->width, surface->height);
		return TPL_TRUE;
	}

error:
	free(tpl_tbm_surface);
	surface->backend.data = NULL;

	return TPL_FALSE;
}

static void
__tpl_tbm_surface_fini(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	TPL_LOG(3, "%s(%p, %p)",
			((surface->type == TPL_SURFACE_TYPE_WINDOW)?"window":"pixmap"),
			 surface,
			 surface->native_handle);
	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
		tbm_surface_internal_unref((tbm_surface_h)surface->native_handle);
	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		/*TODO: we need fix for dequeued surface*/
	}

	free(surface->backend.data);
	surface->backend.data = NULL;
}

static void
__tpl_tbm_surface_post(tpl_surface_t *surface, tbm_surface_h tbm_surface)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);
	TPL_ASSERT(tbm_surface);

	tbm_surface_internal_unref(tbm_surface);

	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		TPL_WARN("Pixmap cannot post(%p, %p)",surface, surface->native_handle);
		return;
	}

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	tbm_surface_queue_h tbm_queue = (tbm_surface_queue_h)surface->native_handle;

	if (tbm_queue)
	{
		tbm_surface_queue_enqueue(tbm_queue, tbm_surface);
		TPL_LOG(6, "tbm_surface ENQUEUED!!");
	}
}

static tpl_bool_t
__tpl_tbm_surface_validate(tpl_surface_t *surface)
{
	TPL_IGNORE(surface);

	return TPL_TRUE;
}

static tbm_surface_h
__tpl_tbm_surface_get_buffer_from_tbm_queue(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
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
	if(tbm_surface == NULL && tbm_surface_queue_can_dequeue(tbm_queue, 1) == 1)
	{
		tsq_err = tbm_surface_queue_dequeue(tbm_queue, &tbm_surface);
		if (tbm_surface == NULL)
		{
			TPL_ERR("Failed to get tbm_surface from tbm_surface_queue | tsq_err = %d",tsq_err);
			return NULL;
		}
	}

	return tbm_surface;
}

static tbm_surface_h
__tpl_tbm_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tbm_surface_h tbm_surface = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->backend.data);

	if (reset_buffers != NULL)
		*reset_buffers = TPL_FALSE;

	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		tbm_surface = __tpl_tbm_surface_get_buffer_from_tbm_queue(surface, reset_buffers);
	}
	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		tbm_surface = (tbm_surface_h)surface->native_handle;
	}

	TPL_ASSERT(tbm_surface);

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

	if (native_dpy == NULL)
		return TPL_FALSE;

	bufmgr = tbm_bufmgr_init(-1);
	if (bufmgr == (tbm_bufmgr)native_dpy)
		ret = TPL_TRUE;

	if (bufmgr)
		tbm_bufmgr_deinit(bufmgr);

	return ret;
}

void
__tpl_display_init_backend_tbm(tpl_display_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_TBM;
	backend->data = NULL;

	backend->init				= __tpl_tbm_display_init;
	backend->fini				= __tpl_tbm_display_fini;
	backend->query_config			= __tpl_tbm_display_query_config;
	backend->filter_config			= __tpl_tbm_display_filter_config;
	backend->get_window_info		= __tpl_tbm_display_get_window_info;
	backend->get_pixmap_info		= __tpl_tbm_display_get_pixmap_info;
	backend->flush				= __tpl_tbm_display_flush;
}

void
__tpl_surface_init_backend_tbm(tpl_surface_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_TBM;
	backend->data = NULL;

	backend->init		= __tpl_tbm_surface_init;
	backend->fini		= __tpl_tbm_surface_fini;
	backend->validate	= __tpl_tbm_surface_validate;
	backend->get_buffer	= __tpl_tbm_surface_get_buffer;
	backend->post		= __tpl_tbm_surface_post;
}

