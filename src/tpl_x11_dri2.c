#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xlib-xcb.h>

#include <libdrm/drm.h>
#include <xf86drm.h>

#include <dri2/dri2.h>
#include <tbm_bufmgr.h>

#include "tpl_internal.h"

#include "tpl_x11_internal.h"


typedef struct _tpl_x11_dri2_surface	tpl_x11_dri2_surface_t;


struct _tpl_x11_dri2_surface
{
	int		latest_post_interval;
	XserverRegion	damage;
	tpl_list_t	buffer_cache;
	tpl_buffer_t	*latest_render_target;
};



static tpl_x11_global_t	global =
{
	0,
	NULL,
	-1,
	NULL,
	TPL_X11_SWAP_TYPE_ASYNC,
	TPL_X11_SWAP_TYPE_SYNC
};

static Display *
__tpl_x11_dri2_get_worker_display(void)
{
	Display *display;
	pthread_mutex_t mutex = __tpl_x11_get_global_mutex();

	pthread_mutex_lock(&mutex);
	TPL_ASSERT(global.display_count > 0);

	/* Use dummy display for worker thread. :-) */
	display = global.worker_display;

	pthread_mutex_unlock(&mutex);

	return display;
}

static void
__tpl_x11_dri2_surface_post_internal(tpl_surface_t *surface, tpl_frame_t *frame,
				tpl_bool_t is_worker)
{
	Display *display;
	Drawable drawable;
	CARD64 swap_count;
	tpl_x11_dri2_surface_t *x11_surface;
	XRectangle *xrects;
	XRectangle xrects_stack[TPL_STACK_XRECTANGLE_SIZE];
	int interval = frame->interval;

	x11_surface = (tpl_x11_dri2_surface_t *)surface->backend.data;

	if (is_worker)
		display = __tpl_x11_dri2_get_worker_display();
	else
		display = surface->display->native_handle;

	drawable = (Drawable)surface->native_handle;

	if (interval < 1)
		interval = 1;

	if (interval != x11_surface->latest_post_interval)
	{
		DRI2SwapInterval(display, drawable, interval);
		x11_surface->latest_post_interval = interval;
	}

	if (tpl_region_is_empty(&frame->damage))
	{
		DRI2SwapBuffers(display, drawable, 0, 0, 0, &swap_count);
	}
	else
	{
		int i;

		if (frame->damage.num_rects > TPL_STACK_XRECTANGLE_SIZE)
		{
			xrects = (XRectangle *)malloc(sizeof(XRectangle) *
						      frame->damage.num_rects);
		}
		else
		{
			xrects = &xrects_stack[0];
		}

		for (i = 0; i < frame->damage.num_rects; i++)
		{
			const int *rects = &frame->damage.rects[i * 4];

			xrects[i].x		= rects[0];
			xrects[i].y		= frame->buffer->height - rects[1] - rects[3];
			xrects[i].width		= rects[2];
			xrects[i].height	= rects[3];
		}

		if (x11_surface->damage == None)
		{
			x11_surface->damage =
				XFixesCreateRegion(display, xrects, frame->damage.num_rects);
		}
		else
		{
			XFixesSetRegion(display, x11_surface->damage,
					xrects, frame->damage.num_rects);
		}

		DRI2SwapBuffersWithRegion(display, drawable, x11_surface->damage, &swap_count);
	}

	frame->state = TPL_FRAME_STATE_POSTED;
}

static tpl_bool_t
__tpl_x11_dri2_display_init(tpl_display_t *display)
{
	pthread_mutex_t mutex = __tpl_x11_get_global_mutex();
	if (display->native_handle == NULL)
	{
		display->native_handle = XOpenDisplay(NULL);
		TPL_ASSERT(display->native_handle != NULL);
	}
    display->xcb_connection = XGetXCBConnection( (Display*)display->native_handle );
    if( NULL == display->xcb_connection )
	{
		TPL_WARN("XGetXCBConnection failed");
	}

	pthread_mutex_lock(&mutex);

	if (global.display_count == 0)
	{
		Bool xres = False;
		char *drv = NULL;
		char *dev = NULL;
		int major = -1;
		int minor = -1;
		int event_base = -1;
		int error_base = -1;
		Window root = 0;
		drm_magic_t magic;

		/* Open a dummy display connection. */
		global.worker_display = XOpenDisplay(NULL);
		TPL_ASSERT(global.worker_display != NULL);

		/* Get default root window. */
		root = DefaultRootWindow(global.worker_display);

		/* Initialize DRI2. */
		xres = DRI2QueryExtension(global.worker_display, &event_base, &error_base);
		TPL_ASSERT(xres == True);

		xres = DRI2QueryVersion(global.worker_display, &major, &minor);
		TPL_ASSERT(xres == True);

		xres = DRI2Connect(global.worker_display, root, &drv, &dev);
		TPL_ASSERT(xres == True);

		/* Initialize buffer manager. */
		global.bufmgr_fd = open(dev, O_RDWR);
		drmGetMagic(global.bufmgr_fd, &magic);
		global.bufmgr = tbm_bufmgr_init(global.bufmgr_fd);

		/* DRI2 authentication. */
		xres = DRI2Authenticate(global.worker_display, root, magic);
		TPL_ASSERT(xres == True);

		/* Initialize swap type configuration. */
		__tpl_x11_swap_str_to_swap_type(getenv(EGL_X11_WINDOW_SWAP_TYPE_ENV_NAME),
						     &global.win_swap_type);

		__tpl_x11_swap_str_to_swap_type(getenv(EGL_X11_FB_SWAP_TYPE_ENV_NAME),
						     &global.fb_swap_type);
	}

	global.display_count++;
	display->bufmgr_fd = global.bufmgr_fd;

	pthread_mutex_unlock(&mutex);
	return TPL_TRUE;
}

static void
__tpl_x11_dri2_display_fini(tpl_display_t *display)
{

	pthread_mutex_t mutex = __tpl_x11_get_global_mutex();
	TPL_IGNORE(display);
	pthread_mutex_lock(&mutex);

	if (--global.display_count == 0)
	{
		tbm_bufmgr_deinit(global.bufmgr);
		close(global.bufmgr_fd);
		XCloseDisplay(global.worker_display);

		global.worker_display = NULL;
		global.bufmgr_fd = -1;
		global.bufmgr = NULL;
	}

	pthread_mutex_unlock(&mutex);

}

static tpl_bool_t
__tpl_x11_dri2_surface_init(tpl_surface_t *surface)
{
	Display *display;
	Drawable drawable;
	tpl_x11_dri2_surface_t *x11_surface;
	tpl_format_t format = TPL_FORMAT_INVALID;

	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		if (!__tpl_x11_display_get_window_info(surface->display, surface->native_handle,
						       &surface->width, &surface->height, NULL,0,0))
			return TPL_FALSE;
	}
	else
	{
		if (!__tpl_x11_display_get_pixmap_info(surface->display, surface->native_handle,
						       &surface->width, &surface->height, &format))
			return TPL_FALSE;
	}

	x11_surface = (tpl_x11_dri2_surface_t *)calloc(1, sizeof(tpl_x11_dri2_surface_t));

	if (x11_surface == NULL)
	{
		TPL_ASSERT(TPL_FALSE);
		return TPL_FALSE;
	}

	x11_surface->latest_post_interval = -1;
	tpl_list_init(&x11_surface->buffer_cache);

	display = (Display *)surface->display->native_handle;
	drawable = (Drawable)surface->native_handle;
	DRI2CreateDrawable(display, drawable);

	surface->backend.data = (void *)x11_surface;

	return TPL_TRUE;
}

static void
__tpl_x11_dri2_surface_fini(tpl_surface_t *surface)
{
	Display *display;
	Drawable drawable;
	tpl_x11_dri2_surface_t *x11_surface;

	display = (Display *)surface->display->native_handle;
	drawable = (Drawable)surface->native_handle;
	x11_surface = (tpl_x11_dri2_surface_t *)surface->backend.data;

	if (x11_surface)
	{
		__tpl_x11_surface_buffer_cache_clear(&x11_surface->buffer_cache);

		if (x11_surface->damage)
			XFixesDestroyRegion(display, x11_surface->damage);

		free(x11_surface);
	}

	DRI2DestroyDrawable(display, drawable);
	surface->backend.data = NULL;
}


static void
__tpl_x11_dri2_surface_post(tpl_surface_t *surface, tpl_frame_t *frame)
{
	__tpl_x11_dri2_surface_post_internal(surface, frame, TPL_TRUE);
}

static void
__tpl_x11_surface_begin_frame(tpl_surface_t *surface)
{
	tpl_frame_t *prev_frame;

	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return;

	prev_frame = __tpl_surface_get_latest_frame(surface);

	if (prev_frame && prev_frame->state != TPL_FRAME_STATE_POSTED)
	{
		if ((DRI2_BUFFER_IS_FB(prev_frame->buffer->backend.flags) &&
		     global.fb_swap_type == TPL_X11_SWAP_TYPE_SYNC) ||
		    (!DRI2_BUFFER_IS_FB(prev_frame->buffer->backend.flags) &&
		     global.win_swap_type == TPL_X11_SWAP_TYPE_SYNC))
		{
			__tpl_surface_wait_all_frames(surface);
		}
	}
}

static tpl_bool_t
__tpl_x11_surface_validate_frame(tpl_surface_t *surface)
{
	tpl_x11_dri2_surface_t *x11_surface = (tpl_x11_dri2_surface_t *)surface->backend.data;

	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return TPL_TRUE;

	if (surface->frame == NULL)
		return TPL_TRUE;

	if ((DRI2_BUFFER_IS_FB(surface->frame->buffer->backend.flags) &&
	     global.fb_swap_type == TPL_X11_SWAP_TYPE_LAZY) ||
	    (!DRI2_BUFFER_IS_FB(surface->frame->buffer->backend.flags) &&
	     global.win_swap_type == TPL_X11_SWAP_TYPE_LAZY))
	{
		if (x11_surface->latest_render_target == surface->frame->buffer)
		{
			__tpl_surface_wait_all_frames(surface);
			return TPL_FALSE;
		}
	}

	return TPL_TRUE;
}

static void
__tpl_x11_surface_end_frame(tpl_surface_t *surface)
{
	tpl_frame_t *frame = __tpl_surface_get_latest_frame(surface);
	tpl_x11_dri2_surface_t *x11_surface = (tpl_x11_dri2_surface_t *)surface->backend.data;

	if (frame)
	{
		x11_surface->latest_render_target = frame->buffer;

		if ((DRI2_BUFFER_IS_FB(frame->buffer->backend.flags) &&
		     global.fb_swap_type == TPL_X11_SWAP_TYPE_ASYNC) ||
		    (!DRI2_BUFFER_IS_FB(frame->buffer->backend.flags) &&
		     global.win_swap_type == TPL_X11_SWAP_TYPE_ASYNC))
		{
			__tpl_x11_dri2_surface_post_internal(surface, frame, TPL_FALSE);
		}
	}
}

static tpl_buffer_t *
__tpl_x11_dri2_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	Display *display;
	Drawable drawable;
	DRI2Buffer *dri2_buffers;
	uint32_t attachments[1] = { DRI2BufferBackLeft };
	tbm_bo bo;
	tbm_bo_handle bo_handle;
	int width, height, num_buffers;
	tpl_x11_dri2_surface_t *x11_surface = (tpl_x11_dri2_surface_t *)surface->backend.data;

	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
		attachments[0] = DRI2BufferFrontLeft;

	display = (Display *)surface->display->native_handle;
	drawable = (Drawable)surface->native_handle;

	/* Get the current buffer via DRI2. */
	dri2_buffers = DRI2GetBuffers(display, drawable,
				      &width, &height, attachments, 1, &num_buffers);
	if (dri2_buffers == NULL)
		goto err_buffer;

	if (DRI2_BUFFER_IS_REUSED(dri2_buffers[0].flags))
	{
		/* Buffer is reused. So it should be in the buffer cache.
		 * However, sometimes we get a strange result of having reused flag for a newly
		 * received buffer. I don't know the meaning of such cases but just handle it. */
		buffer = __tpl_x11_surface_buffer_cache_find(&x11_surface->buffer_cache, dri2_buffers[0].name);

		if (buffer)
		{
			/* Need to update buffer flag */
			buffer->backend.flags = dri2_buffers[0].flags;
			/* just update the buffer age. */
#if (TIZEN_FEATURES_ENABLE)
			buffer->age = DRI2_BUFFER_GET_AGE(dri2_buffers[0].flags);
#endif
			goto done;
		}
	}
	else
	{
		/* Buffer configuration of the server is changed. We have to reset all previsouly
		 * received buffers. */
		__tpl_x11_surface_buffer_cache_clear(&x11_surface->buffer_cache);
	}

	/* Create a TBM buffer object for the buffer name. */
	bo = tbm_bo_import(global.bufmgr, dri2_buffers[0].name);

	if (bo == NULL)
	{
		TPL_ASSERT(TPL_FALSE);
		goto done;
	}

	bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);

	/* Create tpl buffer. */
	buffer = __tpl_buffer_alloc(surface, dri2_buffers[0].name, (int)bo_handle.u32,
				    width, height, dri2_buffers[0].cpp * 8, dri2_buffers[0].pitch);

#if (TIZEN_FEATURES_ENABLE)
	buffer->age = DRI2_BUFFER_GET_AGE(dri2_buffers[0].flags);
#endif
	buffer->backend.data = (void *)bo;
	buffer->backend.flags = dri2_buffers[0].flags;

	/* Add the buffer to the buffer cache. The cache will hold a reference to the buffer. */
	__tpl_x11_surface_buffer_cache_add(&x11_surface->buffer_cache, buffer);
	tpl_object_unreference(&buffer->base);

done:
	if (reset_buffers)
	{
		/* Users use this output value to check if they have to reset previous buffers. */
		*reset_buffers = !DRI2_BUFFER_IS_REUSED(dri2_buffers[0].flags) ||
			width != surface->width || height != surface->height;
	}

	XFree(dri2_buffers);
err_buffer:
	return buffer;
}

tpl_bool_t
__tpl_display_choose_backend_x11_dri2(tpl_handle_t native_dpy)
{
	/* X11 display accepts any type of handle. So other backends must be choosen before this. */
	return TPL_TRUE;
}

void
__tpl_display_init_backend_x11_dri2(tpl_display_backend_t *backend)
{
	backend->type = TPL_BACKEND_X11_DRI2;
	backend->data = NULL;

	backend->init			    = __tpl_x11_dri2_display_init;
	backend->fini			    = __tpl_x11_dri2_display_fini;
	backend->query_config		= __tpl_x11_display_query_config;
	backend->get_window_info	= __tpl_x11_display_get_window_info;
	backend->get_pixmap_info	= __tpl_x11_display_get_pixmap_info;
	backend->flush			    = __tpl_x11_display_flush;
    backend->wait_native        = __tpl_x11_display_wait_native;
}

void
__tpl_surface_init_backend_x11_dri2(tpl_surface_backend_t *backend)
{
	backend->type = TPL_BACKEND_X11_DRI2;
	backend->data = NULL;

	backend->init		    = __tpl_x11_dri2_surface_init;
	backend->fini		    = __tpl_x11_dri2_surface_fini;
	backend->begin_frame    = __tpl_x11_surface_begin_frame;
	backend->end_frame	    = __tpl_x11_surface_end_frame;
	backend->validate_frame	= __tpl_x11_surface_validate_frame;
	backend->get_buffer	    = __tpl_x11_dri2_surface_get_buffer;
	backend->post		    = __tpl_x11_dri2_surface_post;
}

void
__tpl_buffer_init_backend_x11_dri2(tpl_buffer_backend_t *backend)
{
	backend->type = TPL_BACKEND_X11_DRI2;
	backend->data = NULL;

	backend->init		= __tpl_x11_buffer_init;
	backend->fini		= __tpl_x11_buffer_fini;
	backend->map		= __tpl_x11_buffer_map;
	backend->unmap		= __tpl_x11_buffer_unmap;
	backend->lock		= __tpl_x11_buffer_lock;
	backend->unlock		= __tpl_x11_buffer_unlock;
}
