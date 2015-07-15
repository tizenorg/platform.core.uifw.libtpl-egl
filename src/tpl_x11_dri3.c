#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>

#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xfixes.h>


#include <libdrm/drm.h>
#include <xf86drm.h>

#include <X11/xshmfence.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/xcbext.h>
#include <xcb/present.h>
#include <xcb/sync.h>

#include <tbm_bufmgr.h>


#include "tpl_internal.h"

#include "tpl_x11_internal.h"

static int dri3_max_back = 0;/*max number of back buffer*/
#define DRI3_NUM_BUFFERS	20
#define DRI3_BUFFER_REUSED	0x08

#define MALI_DEBUG_PRINT(string,args) do {} while(0)

typedef struct _tpl_x11_dri3_surface	tpl_x11_dri3_surface_t;

struct _tpl_x11_dri3_surface
{
	int		latest_post_interval;
	XserverRegion	damage;
	tpl_list_t	buffer_cache;
	tpl_buffer_t	*latest_render_target;

	void		*drawable;
};

enum dri3_buffer_type
{
	dri3_buffer_back = 0,
	dri3_buffer_front = 1
};

typedef struct _dri3_buffer
{
	tbm_bo		tbo;
	uint32_t	pixmap;
	uint32_t	sync_fence;	/* XID of X SyncFence object */
	struct xshmfence *shm_fence;	/* pointer to xshmfence object */
	uint32_t	busy;		/* Set on swap, cleared on IdleNotify */
	void		*driverPrivate;

	/*param of buffer */
	uint32_t	size;
	uint32_t	pitch;
	uint32_t	cpp;
	uint32_t	flags;
	int32_t		width, height, depth;
	uint64_t	last_swap;
	int32_t		own_pixmap;	/* We allocated the pixmap ID,
					   free on destroy */
	uint32_t	dma_buf_fd;	/* fd of dma buffer */
	/* [BEGIN: 20141125-xuelian.bai] Add old dma fd to save old fd
	 * before use new fd */
	uint32_t	old_dma_fd;
	/* [END: 20141125-xuelian.bai] */
	enum dri3_buffer_type buffer_type; /* back=0,front=1 */
} dri3_buffer;

typedef struct _dri3_drawable
{
	Display		*dpy;
	XID		xDrawable;

	tbm_bufmgr 	bufmgr;		/* tbm bufmgr */

	int32_t		width, height, depth;
	int32_t		swap_interval;
	uint8_t		have_back;
	uint8_t		have_fake_front;
	tpl_bool_t	is_pixmap;	/*whether the drawable is pixmap*/
	uint8_t		flipping;	/*whether the drawable can use pageFlip*/

	uint32_t	present_capabilities; /* Present extension capabilities*/
	uint64_t	send_sbc;	/* swap buffer counter */
	uint64_t	recv_sbc;
	uint64_t	ust, msc;	/* Last received UST/MSC values */
	uint32_t	send_msc_serial; /* Serial numbers for tracking
					    wait_for_msc events */
	uint32_t	recv_msc_serial;

	dri3_buffer	*buffers[DRI3_NUM_BUFFERS]; /*buffer array of all buffers*/
	int		cur_back;

	uint32_t	stamp;
	xcb_present_event_t	eid;
	xcb_special_event_t 	*special_event;
} dri3_drawable;

typedef struct _dri3_drawable_node
{
	XID		xDrawable;
	dri3_drawable	*drawable;
} dri3_drawable_node;
static tpl_x11_global_t	global =
{
	0,
	NULL,
	-1,
	NULL,
	TPL_X11_SWAP_TYPE_ASYNC,
	TPL_X11_SWAP_TYPE_SYNC
};

static tpl_list_t dri3_drawable_list;
static void
dri3_free_render_buffer(dri3_drawable *pdraw, dri3_buffer *buffer);


static inline void
dri3_fence_reset(dri3_buffer *buffer)
{
	xshmfence_reset(buffer->shm_fence);
}

static inline void
dri3_fence_set(dri3_buffer *buffer)
{
	xshmfence_trigger(buffer->shm_fence);
}

static inline void
dri3_fence_trigger(xcb_connection_t *c, dri3_buffer *buffer)
{
	xcb_sync_trigger_fence(c, buffer->sync_fence);
}

static inline void
dri3_fence_await(xcb_connection_t *c, dri3_buffer *buffer)
{
	xcb_flush(c);
	xshmfence_await(buffer->shm_fence);
}

static inline tpl_bool_t
dri3_fence_triggered(dri3_buffer *buffer)
{
	return xshmfence_query(buffer->shm_fence);
}


/******************************************
  * dri3_handle_present_event
  * Process Present event from xserver
  *****************************************/
void
dri3_handle_present_event(dri3_drawable *priv, xcb_present_generic_event_t *ge)
{
	switch (ge->evtype)
	{
		case XCB_PRESENT_CONFIGURE_NOTIFY:
		{
			xcb_present_configure_notify_event_t *ce = (void *) ge;
			MALI_DEBUG_PRINT(0,
				("%s: XCB_PRESENT_CONFIGURE_NOTIFY\n", __func__));
			priv->width = ce->width;
			priv->height = ce->height;
			break;
		}
		case XCB_PRESENT_COMPLETE_NOTIFY:
		{
			xcb_present_complete_notify_event_t *ce = (void *) ge;
			MALI_DEBUG_PRINT(0,
				("%s: XCB_PRESENT_COMPLETE_NOTIFY\n", __func__));
			/* Compute the processed SBC number from the received
			 * 32-bit serial number merged with the upper 32-bits
		 	 * of the sent 64-bit serial number while checking for
		 	 * wrap
		 	 */
			if (ce->kind == XCB_PRESENT_COMPLETE_KIND_PIXMAP)
			{
				priv->recv_sbc =
					(priv->send_sbc & 0xffffffff00000000LL) |
					ce->serial;
				if (priv->recv_sbc > priv->send_sbc)
					priv->recv_sbc -= 0x100000000;
				switch (ce->mode)
				{
					case XCB_PRESENT_COMPLETE_MODE_FLIP:
						priv->flipping = 1;
						break;
					case XCB_PRESENT_COMPLETE_MODE_COPY:
						priv->flipping = 0;
						break;
				}
			} else
			{
				priv->recv_msc_serial = ce->serial;
			}
			priv->ust = ce->ust;
			priv->msc = ce->msc;
			break;
		}
		case XCB_PRESENT_EVENT_IDLE_NOTIFY:
		{
			xcb_present_idle_notify_event_t *ie = (void *) ge;
			uint32_t b;


			for (b = 0; b < sizeof (priv->buffers) / sizeof (priv->buffers[0]); b++)
			{
				dri3_buffer        *buf = priv->buffers[b];

				if (buf && buf->pixmap == ie->pixmap)
				{
					MALI_DEBUG_PRINT(0,
							("%s: id=%d XCB_PRESENT_EVENT_IDLE_NOTIFY\n", __func__, b));
					buf->busy = 0;
					tbm_bo_unref(priv->buffers[b]->tbo);
				break;
				}
			}
		break;
		}
	}
	free(ge);
 }

 /******************************************************
 * dri3_flush_present_events
 *
 * Process any present events that have been received from the X server
 * called when get buffer or swap buffer
 ******************************************************/
static void
dri3_flush_present_events(dri3_drawable *priv)
{
	xcb_connection_t *c = XGetXCBConnection(priv->dpy);

	/* Check to see if any configuration changes have occurred
	 * since we were last invoked
	 */
	if (priv->special_event)
	{
		xcb_generic_event_t    *ev;

		while ((ev = xcb_poll_for_special_event(c, priv->special_event)) != NULL)
		{
			xcb_present_generic_event_t *ge = (void *) ev;
			dri3_handle_present_event(priv, ge);
		}
	}
}


/** dri3_update_drawable
 *
 * Called the first time we use the drawable and then
 * after we receive present configure notify events to
 * track the geometry of the drawable
 */
static int
dri3_update_drawable(void *loaderPrivate)
{
	dri3_drawable *priv = loaderPrivate;
	xcb_connection_t *c = XGetXCBConnection(priv->dpy);
	xcb_extension_t xcb_present_id = { "Present", 0 };

	/* First time through, go get the current drawable geometry
	 */ /*TODO*/
	if (priv->special_event == NULL)
	{
		xcb_get_geometry_cookie_t geom_cookie;
		xcb_get_geometry_reply_t *geom_reply;
		xcb_void_cookie_t cookie;
		xcb_generic_error_t *error;
		xcb_present_query_capabilities_cookie_t present_capabilities_cookie;
		xcb_present_query_capabilities_reply_t *present_capabilities_reply;


		/* Try to select for input on the window.
		 *
		 * If the drawable is a window, this will get our events
		 * delivered.
		 *
		 * Otherwise, we'll get a BadWindow error back from this
		 * request which will let us know that the drawable is a
		 * pixmap instead.
		 */
		cookie = xcb_present_select_input_checked(c,
				(priv->eid = xcb_generate_id(c)),
				priv->xDrawable,
				XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY|
				XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY|
				XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY);

		present_capabilities_cookie = xcb_present_query_capabilities(c, priv->xDrawable);

		/* Create an XCB event queue to hold present events outside of the usual
		 * application event queue
		 */
		priv->special_event = xcb_register_for_special_xge(c,
				&xcb_present_id,
				priv->eid,
				&priv->stamp);

		geom_cookie = xcb_get_geometry(c, priv->xDrawable);

		geom_reply = xcb_get_geometry_reply(c, geom_cookie, NULL);
		TPL_ASSERT(geom_reply != NULL);

		priv->width = geom_reply->width;
		priv->height = geom_reply->height;
		priv->depth = geom_reply->depth;
		priv->is_pixmap = TPL_FALSE;

		free(geom_reply);

		/* Check to see if our select input call failed. If it failed
		 * with a BadWindow error, then assume the drawable is a pixmap.
		 * Destroy the special event queue created above and mark the
		 * drawable as a pixmap
		 */

		error = xcb_request_check(c, cookie);

		present_capabilities_reply = xcb_present_query_capabilities_reply(c,
				present_capabilities_cookie,
				NULL);

		if (present_capabilities_reply)
		{
			priv->present_capabilities = present_capabilities_reply->capabilities;
			free(present_capabilities_reply);
		} else
			priv->present_capabilities = 0;

		if (error)
		{
			MALI_DEBUG_PRINT(0, ("%s:select input error=%d\n",
						__func__, error->error_code));
			if (error->error_code != BadWindow)
			{
				free(error);
				return TPL_FALSE;
			}
			priv->is_pixmap = TPL_TRUE;
			xcb_unregister_for_special_event(c, priv->special_event);
			priv->special_event = NULL;
		}
	}
	dri3_flush_present_events(priv);
	return TPL_TRUE;
}

/** dri3_get_pixmap_buffer
 *
 * Get the DRM object for a pixmap from the X server
 */
static dri3_buffer *
dri3_get_pixmap_buffer(void *loaderPrivate, Pixmap pixmap)/*TODO:format*/
{
	dri3_drawable *pdraw = loaderPrivate;
	dri3_buffer *buffer = pdraw->buffers[dri3_max_back];
	xcb_dri3_buffer_from_pixmap_cookie_t bp_cookie;
	xcb_dri3_buffer_from_pixmap_reply_t  *bp_reply;
	int *fds;
	Display *dpy;
	xcb_connection_t *c;
	xcb_sync_fence_t sync_fence;
	struct xshmfence *shm_fence;
	int fence_fd;
	tbm_bo tbo = NULL;

	/* Reuse this pixmap buffer if it already exist  */
	if (buffer)
	{
		buffer->flags = DRI3_BUFFER_REUSED;
		tbm_bo_ref(buffer->tbo);
		return buffer;
	}
	dpy = pdraw->dpy;
	c = XGetXCBConnection(dpy);

	buffer = calloc(1, sizeof (dri3_buffer));
	if (!buffer)
		goto no_buffer;

	fence_fd = xshmfence_alloc_shm();
	if (fence_fd < 0)
		goto no_fence;
	shm_fence = xshmfence_map_shm(fence_fd);
	if (shm_fence == NULL)
	{
		close (fence_fd);
		goto no_fence;
	}

	xcb_dri3_fence_from_fd(c,
		pixmap,
		(sync_fence = xcb_generate_id(c)),
		TPL_FALSE,
		fence_fd);

	/* Get an FD for the pixmap object
	 */
	bp_cookie = xcb_dri3_buffer_from_pixmap(c, pixmap);
	bp_reply = xcb_dri3_buffer_from_pixmap_reply(c, bp_cookie, NULL);
	if (!bp_reply)
		goto no_image;
	fds = xcb_dri3_buffer_from_pixmap_reply_fds(c, bp_reply);

	tbo = tbm_bo_import_fd(pdraw->bufmgr,(tbm_fd)(*fds));
	MALI_DEBUG_PRINT(0, ("imported tbo==%x, FUNC:%s\n",tbo,__func__));
	if(NULL == tbo)
	{
		MALI_DEBUG_PRINT(0, ("error:tbo==NULL, FUNC:%s\n",__func__));
	}
	tbm_bo_ref(tbo);/* add a ref when created,and unref in dri3_free_render_buffer */
	buffer->tbo = tbo;
	buffer->dma_buf_fd = *fds;
	buffer->pixmap = pixmap;
	buffer->own_pixmap = TPL_FALSE;
	buffer->width = bp_reply->width;
	buffer->height = bp_reply->height;
    buffer->pitch = bp_reply->width*(bp_reply->bpp/8);
	buffer->buffer_type = dri3_buffer_front;
	buffer->shm_fence = shm_fence;
	buffer->sync_fence = sync_fence;

	pdraw->buffers[dri3_max_back] = buffer;
	return buffer;

no_image:
    MALI_DEBUG_PRINT(0, ("error:no_image,buffer_from_pixmap failed in FUNC:%s",
				__func__));
	xcb_sync_destroy_fence(c, sync_fence);
	xshmfence_unmap_shm(shm_fence);
no_fence:
	free(buffer);
	MALI_DEBUG_PRINT(0, ("error:no_fence,xshmfence_map_shm failed in FUNC:%s",
				__func__));
no_buffer:
	return NULL;
}

/** dri3_find_back
 *
 * Find an idle back buffer. If there isn't one, then
 * wait for a present idle notify event from the X server
 */
static int
dri3_find_back(xcb_connection_t *c, dri3_drawable *priv)
{
	int  b;
	xcb_generic_event_t *ev;
	xcb_present_generic_event_t *ge;


	for (;;)
	{
		for (b = 0; b < dri3_max_back; b++)
		{
			int id = (b + priv->cur_back + 1) % dri3_max_back;
			dri3_buffer *buffer = priv->buffers[id];

			MALI_DEBUG_PRINT(0, ("%s id=%d,buffer=%p\n",
						__func__, id, buffer));
			if (!buffer || !buffer->busy)
			{
				MALI_DEBUG_PRINT(0, ("%s find buffer success:id=%d,buffer=%p\n",
							__func__, id, buffer));
				priv->cur_back = id;
				return id;
			}
		}
		xcb_flush(c);
		ev = xcb_wait_for_special_event(c, priv->special_event);
		if (!ev)
			return -1;
		ge = (void *) ev;
		dri3_handle_present_event(priv, ge);
	}
}

/** dri3_alloc_render_buffer
 *
 *  allocate a render buffer and create an X pixmap from that
 *
 * Allocate an xshmfence for synchronization
 */
static dri3_buffer *
dri3_alloc_render_buffer(dri3_drawable *priv,
		int width, int height, int depth, int cpp)
{
	Display *dpy = priv->dpy;
	Drawable draw = priv->xDrawable;
	dri3_buffer *buffer = NULL;
	xcb_connection_t *c = XGetXCBConnection(dpy);
	xcb_pixmap_t pixmap = 0;
	xcb_sync_fence_t sync_fence;
	struct xshmfence *shm_fence;
	int buffer_fd, fence_fd;
	int size;
	tbm_bo_handle handle;
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;

	/* Create an xshmfence object and
	 * prepare to send that to the X server
	 */
	fence_fd = xshmfence_alloc_shm();
	if (fence_fd < 0)
	{
		MALI_DEBUG_PRINT(0, ("%s:error:xshmfence_alloc_shm failed\n",
					__func__));
		return NULL;
	}

	shm_fence = xshmfence_map_shm(fence_fd);
	if (shm_fence == NULL)
	{
		MALI_DEBUG_PRINT(0, ("%s:error:xshmfence_map_shm failed\n",
					__func__));
		goto no_shm_fence;
	}

	/* Allocate the image from the driver
	 */
	buffer = calloc(1, sizeof (dri3_buffer));
	if (!buffer)
	{
		MALI_DEBUG_PRINT(0, ("%s:error:buffer alloc failed\n",
					__func__));
		goto no_buffer;
	}

	/* size = height * width * depth/8;*/
	/* size = ((width * 32)>>5) * 4 * height; */
	/* [BEGIN: 20141125-xing.huang] calculate pitch and size
	 * by input parameter cpp */
	buffer->pitch = width*(cpp/8);
	size = buffer->pitch*height;
	/* [END:20141125-xing.huang] */

	buffer->tbo = tbm_bo_alloc(priv->bufmgr, size, TBM_BO_DEFAULT);
	if (NULL == buffer->tbo)
	{
		MALI_DEBUG_PRINT(0, ("%s:error: buffer->tbo==NULL\n",
					__func__));
		goto no_buffer;
	}

	/* dup tbo, because X will close it */
	handle = tbm_bo_get_handle(buffer->tbo, TBM_DEVICE_3D);
	buffer_fd = dup(handle.u32);
	buffer->dma_buf_fd = handle.u32;
	buffer->size = size;
	cookie = xcb_dri3_pixmap_from_buffer_checked(c,
			(pixmap = xcb_generate_id(c)),
			draw,
			buffer->size,
			width, height, buffer->pitch,
			depth, cpp,
			buffer_fd);
	error = xcb_request_check( c, cookie);
	if (error)
	{
		MALI_DEBUG_PRINT(0, ("%s: xcb_dri3_pixmap_from_buffer failed, err_code=%d\n",
					__func__, error->error_code));
		goto no_pixmap;
	}
	if (0 == pixmap)
	{
		MALI_DEBUG_PRINT(0, ("%s: error:xcb_dri3_pixmap_from_buffer pixmap=0\n",
				__func__));
		goto no_pixmap;
	}
	cookie = xcb_dri3_fence_from_fd_checked(c,
			pixmap,
			(sync_fence = xcb_generate_id(c)),
			TPL_FALSE,
			fence_fd);
	error = xcb_request_check( c, cookie);
	if (error)
	{
		MALI_DEBUG_PRINT(0, ("%s: xcb_dri3_fence_from_fd failed,err_code=%d\n",
				__func__, error->error_code));
		goto no_pixmap;
	}
	buffer->pixmap = pixmap;
	buffer->own_pixmap = TPL_TRUE;
	buffer->sync_fence = sync_fence;
	buffer->shm_fence = shm_fence;
	buffer->width = width;
	buffer->height = height;
    buffer->depth = depth;
    buffer->cpp = cpp;
	buffer->flags = 0;

	/* Mark the buffer as idle
	 */
	dri3_fence_set(buffer);

	return buffer;
no_pixmap:
	tbm_bo_unref(buffer->tbo);
	free(buffer);
no_buffer:
	xshmfence_unmap_shm(shm_fence);
no_shm_fence:
	close(fence_fd);
	return NULL;
}

/** dri3_free_render_buffer
 *
 * Free everything associated with one render buffer including pixmap, fence
 * stuff
 */
static void
dri3_free_render_buffer(dri3_drawable *pdraw, dri3_buffer *buffer)
{
	xcb_connection_t *c = XGetXCBConnection(pdraw->dpy);

	MALI_DEBUG_PRINT(0, ("%s buffer=%p\n",__func__, buffer));
	if (buffer->own_pixmap)
		xcb_free_pixmap(c, buffer->pixmap);
	xcb_sync_destroy_fence(c, buffer->sync_fence);
	xshmfence_unmap_shm(buffer->shm_fence);
	if (buffer->busy)
		tbm_bo_unref(buffer->tbo);
    /* added a ref when created and unref while free, see dri3_get_pixmap_buffer */
    if(pdraw->is_pixmap)
    {
        tbm_bo_unref(buffer->tbo);
    }
	free(buffer);
    buffer = NULL;
}

/** dri3_free_buffers
 *
 * Free the front buffer or all of the back buffers. Used
 * when the application changes which buffers it needs
 */
 #if 0
static void
dri3_free_buffers(enum dri3_buffer_type buffer_type, void *loaderPrivate)
{
	dri3_drawable	*priv = loaderPrivate;
	dri3_buffer	*buffer;
	int		first_id;
	int		n_id;
	int		buf_id;

	switch (buffer_type)
	{
		case dri3_buffer_back:
			first_id = 0;
			n_id = dri3_max_back;
			break;
		case dri3_buffer_front:
			first_id = dri3_max_back;
			n_id = 1;
	}

	for (buf_id = first_id; buf_id < first_id + n_id; buf_id++)
	{
		buffer = priv->buffers[buf_id];
		if (buffer)
		{
			dri3_free_render_buffer(priv, buffer);
			priv->buffers[buf_id] = NULL;
		}
	}
}
#endif

/** dri3_get_window_buffer
 *
 * Find a front or back buffer, allocating new ones as necessary
 */
static dri3_buffer *
dri3_get_window_buffer(void *loaderPrivate, int cpp)
{
	dri3_drawable		*priv = loaderPrivate;
	xcb_connection_t	*c = XGetXCBConnection(priv->dpy);
	dri3_buffer		*backbuffer = NULL;
	int			back_buf_id,reuse = 1;
	uint32_t		old_dma_fd = 0;
	back_buf_id = dri3_find_back(c, priv);

	backbuffer = priv->buffers[back_buf_id];


	/* Allocate a new buffer if there isn't an old one, or if that
	 * old one is the wrong size.
	 */
	if (!backbuffer || backbuffer->width != priv->width ||
			backbuffer->height != priv->height)
	{
		dri3_buffer   *new_buffer;

		/* Allocate the new buffers
		 */
		new_buffer = dri3_alloc_render_buffer(priv,
				priv->width, priv->height, priv->depth, cpp);

		if (!new_buffer)
		{
			MALI_DEBUG_PRINT(0, ("%s:error: alloc new buffer failed\n",
						__func__));
			return NULL;
		}
		reuse = 0;
		/* When resizing, copy the contents of the old buffer,
		 * waiting for that copy to complete using our fences
		 * before proceeding
		 */
		if (backbuffer)
		{
			dri3_fence_reset(new_buffer);
			dri3_fence_await(c, backbuffer);
			/* [BEGIN: 20141125-xuelian.bai] Size not match,this buffer
			 * must be removed from buffer cache, so we have to save
			 * dma_buf_fd of old buffer.*/
			old_dma_fd = backbuffer->dma_buf_fd;
			/* [END: 20141125-xuelian.bai] */
			dri3_free_render_buffer(priv, backbuffer);
		}
		backbuffer = new_buffer;
		backbuffer->buffer_type = dri3_buffer_back;
		backbuffer->old_dma_fd = old_dma_fd;/*save dma_buf_fd of old buffer*/
		priv->buffers[back_buf_id] = backbuffer;
		goto no_need_wait;/* Skip dri3_fence_await */
	}

	dri3_fence_await(c, backbuffer);
no_need_wait:
	backbuffer->flags = DRI2_BUFFER_FB;
	if (!reuse)
	{
		MALI_DEBUG_PRINT(0, ("%s:allocate new buffer\n", __func__));
	}
	else
	{
		backbuffer->flags |= DRI2_BUFFER_REUSED;
		MALI_DEBUG_PRINT(0, ("%s:reuse old buffer\n", __func__));
	}
	backbuffer->busy = 1;
	tbm_bo_ref(backbuffer->tbo);

	/* Return the requested buffer */
	return backbuffer;
}

static dri3_buffer *dri3_get_buffers(XID drawable,  void *loaderPrivate,
		int *width, int *height, unsigned int *attachments,
		int count, int *out_count, int cpp)
{
	dri3_drawable *priv = loaderPrivate;
	dri3_buffer *buffers = NULL;

	MALI_DEBUG_PRINT(0, ("%s:begin\n",__func__));

	if (drawable != priv->xDrawable)
	{
		MALI_DEBUG_PRINT(0, ("%s error:drawable mismatch\n", __func__));
		return NULL;
	}

	if (!dri3_update_drawable(loaderPrivate))
	{
		MALI_DEBUG_PRINT(0, ("%s dri3_update_drawable filed\n", __func__));
		return NULL;
	}

	/*buffers = calloc((count + 1), sizeof(buffers[0]));

	if (!buffers)
		return NULL;*//*TODO*/

	if (*attachments == dri3_buffer_front)
		buffers = dri3_get_pixmap_buffer(loaderPrivate, priv->xDrawable);
	else
		buffers = dri3_get_window_buffer(loaderPrivate, cpp);

	*out_count = 1;
	*width = (int)buffers->width;
	*height = (int)buffers->height;

	MALI_DEBUG_PRINT(0, ("%s end\n",__func__));
	return buffers;
}

/******************************************************
 * dri3_swap_buffers
 * swap back buffer with front buffer
 * Make the current back buffer visible using the present extension
 * if (region_t==0),swap whole frame, else swap with region
 ******************************************************/
static int64_t
dri3_swap_buffers(Display *dpy, void *priv, int interval, XID region_t)
{

	int64_t		ret = -1;
	int64_t		target_msc = 0;
	int64_t		divisor = 0;
	int64_t		remainder = 0;
	xcb_connection_t *c = XGetXCBConnection(dpy);
	dri3_drawable	*pDrawable = (dri3_drawable*)priv;
	dri3_buffer	*back = NULL;

	MALI_DEBUG_PRINT(0, ("\n#########%s begin######\n",__func__));


	back = pDrawable->buffers[pDrawable->cur_back];

	/* Process any present events that have been received from the X
	 * server
	 */
	dri3_flush_present_events(pDrawable);

	if ((back == NULL)||(pDrawable == NULL)||(pDrawable->is_pixmap != 0))
	{
		MALI_DEBUG_PRINT(0, ("%s error:input error:\n",__func__));
		MALI_DEBUG_PRINT(0, ("\t back=%p,pDrawable=%p,pDrawable->is_pixmap=%d\n",
				back,pDrawable,pDrawable->is_pixmap));
		return ret;
	}

	dri3_fence_reset(back);

	/* Compute when we want the frame shown by taking the last known
	 * successful MSC and adding in a swap interval for each outstanding
	 * swap request
	 */
	if (pDrawable->swap_interval != interval)
		pDrawable->swap_interval = interval;

	++pDrawable->send_sbc;
	if (target_msc == 0)
		target_msc = pDrawable->msc + pDrawable->swap_interval *
			(pDrawable->send_sbc - pDrawable->recv_sbc);

	/* set busy flag */
	back->busy = 1;
	back->last_swap = pDrawable->send_sbc;

	xcb_present_pixmap(c,
			pDrawable->xDrawable,		/* dst */
			back->pixmap,			/* src */
			(uint32_t) pDrawable->send_sbc,
			0,				/* valid */
			region_t,			/* update */
			0,				/* x_off */
			0,				/* y_off */
			None,				/* target_crtc */
			None,
			back->sync_fence,
			XCB_PRESENT_OPTION_NONE,
			target_msc,
			divisor,
			remainder, 0, NULL);

	ret = (int64_t) pDrawable->send_sbc;
	if (ret == -1)
		MALI_DEBUG_PRINT(0, ("%s swap failed\n",__func__));
	else
		MALI_DEBUG_PRINT(0, ("######%s finish! send_sbc=%d#######\n\n",
					__func__, ret));

	xcb_flush(c);

	++(pDrawable->stamp);
	return ret;
}


/* Wrapper around xcb_dri3_open*/
static int
dri3_open(Display *dpy, Window root, CARD32 provider)
{
	xcb_dri3_open_cookie_t	cookie;
	xcb_dri3_open_reply_t	*reply;
	xcb_connection_t	*c = XGetXCBConnection(dpy);
	int			fd;

	MALI_DEBUG_PRINT(0, ("\n--------%s begin-------\n",__func__));
	cookie = xcb_dri3_open(c,
			root,
			provider);

	reply = xcb_dri3_open_reply(c, cookie, NULL);
	if (!reply)
	{
		MALI_DEBUG_PRINT(0, ("%s xcb_dri3_open failed\n", __func__));
		return -1;
	}

	if (reply->nfd != 1)
	{
		MALI_DEBUG_PRINT(0, ("%s xcb_dri3_open reply error\n", __func__));
		free(reply);
		return -1;
	}

	fd = xcb_dri3_open_reply_fds(c, reply)[0];
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (0 == fd)
		MALI_DEBUG_PRINT(0, ("%s error: fd=0\n",__func__));
	else
		MALI_DEBUG_PRINT(0, ("%s open successfully fd=%d:\n",__func__, fd));

	free(reply);
	MALI_DEBUG_PRINT(0, ("\n---------%s end---------\n",__func__));
	return fd;
}

static void *
dri3_create_drawable(Display *dpy, XID xDrawable)
{
	dri3_drawable			*pdraw = NULL;
	xcb_connection_t		*c = XGetXCBConnection(dpy);
	xcb_get_geometry_cookie_t	geom_cookie;
	xcb_get_geometry_reply_t	*geom_reply;
	int i;
	tpl_list_node_t 		*node;
	dri3_drawable_node 		*drawable_node;


	MALI_DEBUG_PRINT(0, ("\n--------%s begin-------\n",__func__));

	/* Check drawable list to find that if it has been created*/
	node = tpl_list_get_front_node(&dri3_drawable_list);
	while (node)
	{
		dri3_drawable_node *drawable = (dri3_drawable_node *)tpl_list_node_get_data(node);

		if (drawable->xDrawable == xDrawable)
		{
			pdraw = drawable->drawable;
			return (void *)pdraw;/* Reuse old drawable */
		}
		node = tpl_list_node_next(node);
	}
	pdraw = calloc(1, sizeof(*pdraw));
	TPL_ASSERT(pdraw != NULL);

	geom_cookie = xcb_get_geometry(c, xDrawable);
	geom_reply = xcb_get_geometry_reply(c, geom_cookie, NULL);
	TPL_ASSERT(geom_reply != NULL);

	pdraw->bufmgr = global.bufmgr;
	pdraw->width = geom_reply->width;
	pdraw->height = geom_reply->height;
	pdraw->depth = geom_reply->depth;
	pdraw->is_pixmap = TPL_FALSE;

	free(geom_reply);
	pdraw->dpy = dpy;
	pdraw->xDrawable = xDrawable;

	for (i = 0; i < dri3_max_back + 1;i++)
		pdraw->buffers[i] = NULL;


	/* Add new allocated drawable to drawable list */
	drawable_node = calloc(1, sizeof(dri3_drawable_node));
	drawable_node->drawable = pdraw;
	drawable_node->xDrawable = xDrawable;
	tpl_list_push_back(&dri3_drawable_list, (void *)drawable_node);

	MALI_DEBUG_PRINT(0, ("\n---------%s end---------\n",__func__));
	return (void*)pdraw;
}

static tpl_bool_t
dri3_display_init(Display *dpy)
{
	/* Initialize DRI3 & DRM */
	xcb_connection_t		*c = XGetXCBConnection(dpy);
	xcb_dri3_query_version_cookie_t	dri3_cookie;
	xcb_dri3_query_version_reply_t	*dri3_reply;
	xcb_present_query_version_cookie_t	present_cookie;
	xcb_present_query_version_reply_t	*present_reply;
	xcb_generic_error_t		*error;
	const xcb_query_extension_reply_t	*extension;
	xcb_extension_t			xcb_dri3_id = { "DRI3", 0 };
	xcb_extension_t			xcb_present_id = { "Present", 0 };

	MALI_DEBUG_PRINT(0, ("\n---------%s begin---------\n",__func__));
	xcb_prefetch_extension_data(c, &xcb_dri3_id);
	xcb_prefetch_extension_data(c, &xcb_present_id);

	extension = xcb_get_extension_data(c, &xcb_dri3_id);
	if (!(extension && extension->present))
	{
		MALI_DEBUG_PRINT(0, ("%s get dri3 extension failed\n", __func__));
		return TPL_FALSE;
	}

	extension = xcb_get_extension_data(c, &xcb_present_id);
	if (!(extension && extension->present))
	{
		MALI_DEBUG_PRINT(0, ("%s get present extension failed\n", __func__));
		return TPL_FALSE;
	}

	dri3_cookie = xcb_dri3_query_version(c,
			XCB_DRI3_MAJOR_VERSION,
			XCB_DRI3_MINOR_VERSION);
	dri3_reply = xcb_dri3_query_version_reply(c, dri3_cookie, &error);
	if (!dri3_reply)
	{
		MALI_DEBUG_PRINT(0, ("%s query dri3 version failed\n", __func__));
		free(error);
		return TPL_FALSE;
	}
	free(dri3_reply);

	present_cookie = xcb_present_query_version(c,
			XCB_PRESENT_MAJOR_VERSION,
			XCB_PRESENT_MINOR_VERSION);
	present_reply = xcb_present_query_version_reply(c, present_cookie, &error);
	if (!present_reply)
	{
		MALI_DEBUG_PRINT(0, ("%s query present version failed\n", __func__));
		free(error);
		return TPL_FALSE;
	}
	free(present_reply);
	MALI_DEBUG_PRINT(0, ("\n---------%s end---------\n",__func__));
	return TPL_TRUE;
}

static void
dri3_destroy_drawable(Display *dpy, XID xDrawable)
{
	dri3_drawable		*pdraw;
	xcb_connection_t	*c = XGetXCBConnection(dpy);
	int 			i;
	tpl_list_node_t 	*node;
	dri3_drawable_node 	*drawable;
	MALI_DEBUG_PRINT(0, ("\n---------%s begin---------\n",__func__));

	/* Remove drawable from list */
	node =  tpl_list_get_front_node(&dri3_drawable_list);
	while (node)
	{
		drawable = (dri3_drawable_node *)tpl_list_node_get_data(node);

		if (drawable->xDrawable== xDrawable)
		{
			pdraw = drawable->drawable;

			if (!pdraw)
				return;

			for (i = 0; i < dri3_max_back + 1; i++)
			{
				if (pdraw->buffers[i])
					dri3_free_render_buffer(pdraw, pdraw->buffers[i]);
			}

			if (pdraw->special_event)
				xcb_unregister_for_special_event(c, pdraw->special_event);
			free(pdraw);
			pdraw = NULL;
			tpl_list_remove(node, free);
			return;
		}

		node = tpl_list_node_next(node);
	}

	/* If didn't find the drawable, means it is already free*/
	MALI_DEBUG_PRINT(0, ("\n---------%s end---------\n",__func__));
	return;
}

static Display *
__tpl_x11_dri3_get_worker_display()
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

static tpl_bool_t
__tpl_x11_dri3_display_init(tpl_display_t *display)
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
		tpl_bool_t xres = TPL_FALSE;
		Window root = 0;
		drm_magic_t magic;

		/* Open a dummy display connection. */
		global.worker_display = XOpenDisplay(NULL);
		TPL_ASSERT(global.worker_display != NULL);

		/* Get default root window. */
		root = DefaultRootWindow(global.worker_display);

		/* Initialize DRI3. */
		xres = dri3_display_init(global.worker_display);
		TPL_ASSERT(xres == TPL_TRUE);


		/* Initialize buffer manager. */
		global.bufmgr_fd = dri3_open(global.worker_display, root, 0);
		drmGetMagic(global.bufmgr_fd, &magic);
		global.bufmgr = tbm_bufmgr_init(global.bufmgr_fd);

		tpl_list_init(&dri3_drawable_list);

		/* Initialize swap type configuration. */
		__tpl_x11_swap_str_to_swap_type(getenv(EGL_X11_WINDOW_SWAP_TYPE_ENV_NAME),
						&global.win_swap_type);

		__tpl_x11_swap_str_to_swap_type(getenv(EGL_X11_FB_SWAP_TYPE_ENV_NAME),
						&global.fb_swap_type);
		/* [BEGIN: 20141125-xuelian.bai] Add env for setting number of back buffers*/
		{
			const char *backend_env;
			int count = 0;
            backend_env = getenv("MALI_EGL_DRI3_BUF_NUM");
            if (!backend_env || strlen(backend_env) == 0)
				dri3_max_back  = 5; /* Default value is 5*/
			else
			{
				count = atoi(backend_env);
				if (count == 1)/* one buffer doesn't work,min is 2 */
					dri3_max_back = 2;
				else if (count < 20)
					dri3_max_back = count;
				else
					dri3_max_back = 5;
			}
		}
		/* [END: 20141125-xuelian.bai] */
	}

	global.display_count++;
	display->bufmgr_fd = global.bufmgr_fd;

	pthread_mutex_unlock(&mutex);
	return TPL_TRUE;
}

static void
__tpl_x11_dri3_display_fini(tpl_display_t *display)
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

		tpl_list_fini(&dri3_drawable_list, NULL);
	}

	pthread_mutex_unlock(&mutex);

}

static tpl_bool_t
__tpl_x11_dri3_surface_init(tpl_surface_t *surface)
{
	Display *display;
	XID drawable;
	tpl_x11_dri3_surface_t *x11_surface;

	x11_surface = (tpl_x11_dri3_surface_t *)calloc(1,
			sizeof(tpl_x11_dri3_surface_t));

	if (x11_surface == NULL)
	{
		TPL_ASSERT(TPL_FALSE);
		return TPL_FALSE;
	}

	x11_surface->latest_post_interval = -1;
	tpl_list_init(&x11_surface->buffer_cache);

	display = (Display *)surface->display->native_handle;
	drawable = (XID)surface->native_handle;

	x11_surface->drawable = dri3_create_drawable(display, drawable);

	surface->backend.data = (void *)x11_surface;
	MALI_DEBUG_PRINT(0, ("%s surface type:%d\n",__func__, surface->type));
	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		__tpl_x11_display_get_window_info(surface->display,
				surface->native_handle,
				&surface->width, &surface->height, NULL,0,0);
	}
	else
	{
		__tpl_x11_display_get_pixmap_info(surface->display,
				surface->native_handle,
				&surface->width, &surface->height, NULL);
	}

	return TPL_TRUE;
}

static void
__tpl_x11_dri3_surface_fini(tpl_surface_t *surface)
{
	Display *display;
	tpl_x11_dri3_surface_t *x11_surface;

	display = (Display *)surface->display->native_handle;
	x11_surface = (tpl_x11_dri3_surface_t *)surface->backend.data;

	dri3_destroy_drawable(display, (XID)surface->native_handle);

	if (x11_surface)
	{
		__tpl_x11_surface_buffer_cache_clear(&x11_surface->buffer_cache);


		if (x11_surface->damage)
			XFixesDestroyRegion(display, x11_surface->damage);

		free(x11_surface);
	}

	surface->backend.data = NULL;
}

static void
__tpl_x11_dri3_surface_post_internal(tpl_surface_t *surface, tpl_frame_t *frame,
		tpl_bool_t is_worker)
{
	Display *display;
	tpl_x11_dri3_surface_t *x11_surface;
	XRectangle *xrects;
	XRectangle xrects_stack[TPL_STACK_XRECTANGLE_SIZE];

	x11_surface = (tpl_x11_dri3_surface_t *)surface->backend.data;

	if (is_worker)
		display = __tpl_x11_dri3_get_worker_display();
	else
		display = surface->display->native_handle;

	if (frame->interval != x11_surface->latest_post_interval)
	{
		x11_surface->latest_post_interval = frame->interval;/*FIXME:set interval?*/
	}

	if (tpl_region_is_empty(&frame->damage))
	{
		dri3_swap_buffers(display, x11_surface->drawable,0,0);/*TODO*/
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

			xrects[i].x	= rects[0];
			xrects[i].y	= frame->buffer->height - rects[1] -
						rects[3];
			xrects[i].width	= rects[2];
			xrects[i].height = rects[3];
		}

		if (x11_surface->damage == None)
		{
			x11_surface->damage =
					XFixesCreateRegion(display, xrects,
						frame->damage.num_rects);
		}
		else
		{
			XFixesSetRegion(display, x11_surface->damage,
					xrects, frame->damage.num_rects);
		}

		dri3_swap_buffers(display, x11_surface->drawable, 0,
				x11_surface->damage);
	}
	frame->state = TPL_FRAME_STATE_POSTED;
}


static void
__tpl_x11_dri3_surface_post(tpl_surface_t *surface, tpl_frame_t *frame)
{
	__tpl_x11_dri3_surface_post_internal(surface, frame, TPL_TRUE);
}

static void
__tpl_x11_dri3_surface_begin_frame(tpl_surface_t *surface)
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
__tpl_x11_dri3_surface_validate_frame(tpl_surface_t *surface)
{
	tpl_x11_dri3_surface_t *x11_surface = (tpl_x11_dri3_surface_t *)surface->backend.data;

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
__tpl_x11_dri3_surface_end_frame(tpl_surface_t *surface)
{
	tpl_frame_t *frame = __tpl_surface_get_latest_frame(surface);
	tpl_x11_dri3_surface_t *x11_surface = (tpl_x11_dri3_surface_t *)surface->backend.data;

	if (frame)
	{
		x11_surface->latest_render_target = frame->buffer;

		if ((DRI2_BUFFER_IS_FB(frame->buffer->backend.flags) &&
		     global.fb_swap_type == TPL_X11_SWAP_TYPE_ASYNC) ||
		    (!DRI2_BUFFER_IS_FB(frame->buffer->backend.flags) &&
		     global.win_swap_type == TPL_X11_SWAP_TYPE_ASYNC))
		{
			__tpl_x11_dri3_surface_post_internal(surface, frame, TPL_FALSE);
		}
	}
}

static tpl_buffer_t *
__tpl_x11_dri3_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;
	Drawable drawable;
	dri3_buffer *buffers = NULL;
	uint32_t attachments[1] = { dri3_buffer_back };
	tbm_bo bo;
	tbm_bo_handle bo_handle;
	int width, height, num_buffers;
	tpl_x11_dri3_surface_t *x11_surface =
			(tpl_x11_dri3_surface_t *)surface->backend.data;
	int cpp = 32;

	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		attachments[0] = dri3_buffer_front;
	}

	drawable = (Drawable)surface->native_handle;

	buffers = dri3_get_buffers(drawable, x11_surface->drawable, &width,
			&height, attachments, 1, &num_buffers, cpp);

	if (DRI2_BUFFER_IS_REUSED(buffers->flags))
	{
		buffer = __tpl_x11_surface_buffer_cache_find(
				&x11_surface->buffer_cache,
				buffers->dma_buf_fd);

		if (buffer)
		{
			/* If the buffer name is reused and there's a cache
			 * entry for that name, just update the buffer age
			 * and return. */
			buffer->age = DRI2_BUFFER_GET_AGE(buffers->flags);
			MALI_DEBUG_PRINT(0, ("%s reuse tplbuffer\n",
								__func__));
			goto done;
		}

	}
	else
	{
		/* Remove the buffer from the cache. */
		__tpl_x11_surface_buffer_cache_remove(
				&x11_surface->buffer_cache,
				buffers->dma_buf_fd);

		/* old_dma_fd stands for the find reused buffer but size not match. 
		 * It must be removed from the list and make a unref. */
		if(buffers->old_dma_fd != 0)
		{
			__tpl_x11_surface_buffer_cache_remove(
					&x11_surface->buffer_cache,
					buffers->old_dma_fd);
		}
	}


	bo = buffers->tbo;


	if (bo == NULL)
	{
		TPL_ASSERT(TPL_FALSE);
		goto done;
	}



	bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);
	MALI_DEBUG_PRINT(0, ("%s dma_buf_fd=%d,handle=%d\n",
					__func__, buffers->dma_buf_fd, bo_handle.u32));

	/* Create tpl buffer. */
	buffer = __tpl_buffer_alloc(surface, buffers->dma_buf_fd,
			(int)bo_handle.u32,
			width, height, buffers->depth, buffers->pitch);

	buffer->age = DRI2_BUFFER_GET_AGE(buffers->flags);
	buffer->backend.data = (void *)bo;
	buffer->backend.flags = buffers->flags;

	__tpl_x11_surface_buffer_cache_add(&x11_surface->buffer_cache, buffer);
	tpl_object_unreference(&buffer->base);

done:
	if (reset_buffers)
	{
		/* Users use this output value to check if they have to reset previous buffers. */
		*reset_buffers = !DRI2_BUFFER_IS_REUSED(buffers->flags) ||
			width != surface->width || height != surface->height;
	}
	/*XFree(buffers);*/
	return buffer;
}

tpl_bool_t
__tpl_display_choose_backend_x11_dri3(tpl_handle_t native_dpy)
{
	/* X11 display accepts any type of handle. So other backends must be choosen before this. */
	return TPL_TRUE;
}

void
__tpl_display_init_backend_x11_dri3(tpl_display_backend_t *backend)
{
	backend->type = TPL_BACKEND_X11_DRI3;
	backend->data = NULL;

	backend->init			= __tpl_x11_dri3_display_init;
	backend->fini			= __tpl_x11_dri3_display_fini;
	backend->query_config		= __tpl_x11_display_query_config;
	backend->get_window_info	= __tpl_x11_display_get_window_info;
	backend->get_pixmap_info	= __tpl_x11_display_get_pixmap_info;
	backend->flush			= __tpl_x11_display_flush;
    backend->wait_native    = __tpl_x11_display_wait_native;
}

void
__tpl_surface_init_backend_x11_dri3(tpl_surface_backend_t *backend)
{
	backend->type = TPL_BACKEND_X11_DRI3;
	backend->data = NULL;

	backend->init		= __tpl_x11_dri3_surface_init;
	backend->fini		= __tpl_x11_dri3_surface_fini;
	backend->begin_frame	= __tpl_x11_dri3_surface_begin_frame;
	backend->end_frame	= __tpl_x11_dri3_surface_end_frame;
	backend->validate_frame	= __tpl_x11_dri3_surface_validate_frame;
	backend->get_buffer	= __tpl_x11_dri3_surface_get_buffer;
	backend->post		= __tpl_x11_dri3_surface_post;
}

void
__tpl_buffer_init_backend_x11_dri3(tpl_buffer_backend_t *backend)
{
	backend->type = TPL_BACKEND_X11_DRI3;
	backend->data = NULL;

	backend->init		= __tpl_x11_buffer_init;
	backend->fini		= __tpl_x11_buffer_fini;
	backend->map		= __tpl_x11_buffer_map;
	backend->unmap		= __tpl_x11_buffer_unmap;
	backend->lock		= __tpl_x11_buffer_lock;
	backend->unlock		= __tpl_x11_buffer_unlock;
}