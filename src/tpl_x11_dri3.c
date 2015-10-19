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

/* 2015-04-15 joonbum.ko@samsung.com */
/* Add macro function for pitch align calculation.*/
#define SIZE_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
#define ALIGNMENT_PITCH_ARGB 64


#define USE_FENCE 0

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

enum dri3_buffer_status
{
        dri3_buffer_idle = 0,
        dri3_buffer_busy = 1,
        dri3_buffer_posted = 2
};

typedef struct _dri3_buffer
{
	tbm_bo		tbo;
	uint32_t	pixmap;
	enum dri3_buffer_status	status;		/* Set on swap, cleared on IdleNotify */
	void		*driverPrivate;

	/*param of buffer */
	uint32_t	size;
	uint32_t	pitch;
	uint32_t	cpp;
	uint32_t	flags;
	int32_t		width, height;
	uint64_t	last_swap;
	int32_t		own_pixmap;	/* We allocated the pixmap ID,
					   free on destroy */
	uint32_t	dma_buf_fd;	/* fd of dma buffer */
	/* [BEGIN: 20141125-xuelian.bai] Add old dma fd to save old fd
	 * before use new fd */
        /* 2015-04-08 joonbum.ko@samsung.com */
        /* Change old buffer name to old_bo_name from old_dma_fd */
	/* uint32_t	old_dma_fd; */
        uint32_t        old_bo_name;
	/* [END: 20141125-xuelian.bai] */
	enum dri3_buffer_type buffer_type; /* back=0,front=1 */

        /* [BEGIN: 20140119-leiba.sun] Add support for buffer age */
        uint32_t        buffer_age;
        /* [END:20150119-leiba.sun] */
} dri3_buffer;

typedef struct _dri3_drawable
{
	Display		*dpy;
	XID		xDrawable;

	tbm_bufmgr	bufmgr;		/* tbm bufmgr */

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
	xcb_special_event_t	*special_event;
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
	TPL_X11_SWAP_TYPE_LAZY,
	TPL_X11_SWAP_TYPE_LAZY
};

static tpl_list_t dri3_drawable_list;
static void
dri3_free_render_buffer(dri3_drawable *pdraw, dri3_buffer *buffer);
static void dri3_flush_present_events(dri3_drawable *priv);
/* Wrapper around xcb_dri3_open*/
static int
dri3_open(Display *dpy, Window root, CARD32 provider)
{
	xcb_dri3_open_cookie_t	cookie;
	xcb_dri3_open_reply_t	*reply;
	xcb_connection_t	*c;
	int			fd;

	TPL_ASSERT(dpy);

	c = XGetXCBConnection(dpy);

	cookie = xcb_dri3_open(c,
			root,
			provider);

	reply = xcb_dri3_open_reply(c, cookie, NULL);
	if (!reply)
	{
		TPL_ERR("XCB DRI3 open failed!");
		return -1;
	}

	if (reply->nfd != 1)
	{
		TPL_ERR("XCB DRI3 open reply failed!");
		free(reply);
		return -1;
	}

	fd = xcb_dri3_open_reply_fds(c, reply)[0];
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	free(reply);
	return fd;
}

static tpl_bool_t
dri3_display_init(Display *dpy)
{
	/* Initialize DRI3 & DRM */
	xcb_connection_t		*c;
	xcb_dri3_query_version_cookie_t	dri3_cookie;
	xcb_dri3_query_version_reply_t	*dri3_reply;
	xcb_present_query_version_cookie_t	present_cookie;
	xcb_present_query_version_reply_t	*present_reply;
	xcb_generic_error_t		*error;
	const xcb_query_extension_reply_t	*extension;
	xcb_extension_t			xcb_dri3_id = { "DRI3", 0 };
	xcb_extension_t			xcb_present_id = { "Present", 0 };

	TPL_ASSERT(dpy);

	c = XGetXCBConnection(dpy);

	xcb_prefetch_extension_data(c, &xcb_dri3_id);
	xcb_prefetch_extension_data(c, &xcb_present_id);

	extension = xcb_get_extension_data(c, &xcb_dri3_id);
	if (!(extension && extension->present))
	{
		TPL_ERR("XCB get extension failed!");
		return TPL_FALSE;
	}

	extension = xcb_get_extension_data(c, &xcb_present_id);
	if (!(extension && extension->present))
	{
		TPL_ERR("XCB get extension failed!");
		return TPL_FALSE;
	}

	dri3_cookie = xcb_dri3_query_version(c,
			XCB_DRI3_MAJOR_VERSION,
			XCB_DRI3_MINOR_VERSION);
	dri3_reply = xcb_dri3_query_version_reply(c, dri3_cookie, &error);
	if (!dri3_reply)
	{
		TPL_ERR("XCB version query failed!");
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
		TPL_ERR("Present version query failed!");
		free(error);
		return TPL_FALSE;
	}
	free(present_reply);
	return TPL_TRUE;
}

static void *
dri3_create_drawable(Display *dpy, XID xDrawable)
{
	dri3_drawable			*pdraw = NULL;
	xcb_connection_t		*c;
	xcb_get_geometry_cookie_t	geom_cookie;
	xcb_get_geometry_reply_t	*geom_reply;
	int i;
	tpl_list_node_t		        *node;
	dri3_drawable_node		*drawable_node;

	TPL_ASSERT(dpy);

	c = XGetXCBConnection(dpy);

	/* Check drawable list to find that if it has been created*/
	node = __tpl_list_get_front_node(&dri3_drawable_list);
	while (node)
	{
		dri3_drawable_node *drawable = (dri3_drawable_node *) __tpl_list_node_get_data(node);

		if (drawable->xDrawable == xDrawable)
		{
			pdraw = drawable->drawable;
			return (void *)pdraw;/* Reuse old drawable */
		}
		node = __tpl_list_node_next(node);
	}
	pdraw = calloc(1, sizeof(*pdraw));
	if (NULL == pdraw)
	{
		TPL_ERR("Failed to allocate memory!");
		return NULL;
	}

	geom_cookie = xcb_get_geometry(c, xDrawable);
	geom_reply = xcb_get_geometry_reply(c, geom_cookie, NULL);
	if (NULL == geom_reply)
	{
		TPL_ERR("XCB get geometry failed!");
		free(pdraw);
		return NULL;
	}

	pdraw->bufmgr = global.bufmgr;
	pdraw->width = geom_reply->width;
	pdraw->height = geom_reply->height;
	pdraw->depth = geom_reply->depth;
	pdraw->is_pixmap = TPL_FALSE;

	free(geom_reply);
	pdraw->dpy = global.worker_display;
	pdraw->xDrawable = xDrawable;

	for (i = 0; i < dri3_max_back + 1;i++)
		pdraw->buffers[i] = NULL;

	/* Add new allocated drawable to drawable list */
	drawable_node = calloc(1, sizeof(dri3_drawable_node));
	if (NULL == drawable_node)
	{
		TPL_ERR("Failed to allocate memory for drawable node!");
		free(pdraw);
		return NULL;
	}

	drawable_node->drawable = pdraw;
	drawable_node->xDrawable = xDrawable;
	if (TPL_TRUE != __tpl_list_push_back(&dri3_drawable_list, (void *)drawable_node))
	{
		TPL_ERR("List operation failed!");
		free(pdraw);
		free(drawable_node);
		return NULL;
	}

	return (void*)pdraw;
}

static void
dri3_destroy_drawable(Display *dpy, XID xDrawable)
{
	dri3_drawable		*pdraw;
	xcb_connection_t	*c;
	int			i;
	tpl_list_node_t	        *node;
	dri3_drawable_node	*drawable;

	TPL_ASSERT(dpy);

	c = XGetXCBConnection(dpy);

	/* Remove drawable from list */
	node =  __tpl_list_get_front_node(&dri3_drawable_list);
	while (node)
	{
		drawable = (dri3_drawable_node *) __tpl_list_node_get_data(node);

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
			__tpl_list_remove(node, free);
			return;
		}

		node = __tpl_list_node_next(node);
	}

	/* If didn't find the drawable, means it is already free*/
	return;
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
	xcb_connection_t *c;
	xcb_extension_t xcb_present_id = { "Present", 0 };

	TPL_ASSERT(priv);
	TPL_ASSERT(priv->dpy);

	c = XGetXCBConnection(priv->dpy);

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
		if (NULL == geom_reply)
		{
			TPL_ERR("Failed to get geometry reply!");
			return TPL_FALSE;
		}

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

/******************************************
  * dri3_handle_present_event
  * Process Present event from xserver
  *****************************************/
static void
dri3_handle_present_event(dri3_drawable *priv, xcb_present_generic_event_t *ge)
{
	TPL_ASSERT(priv);
	TPL_ASSERT(ge);

	switch (ge->evtype)
	{
		case XCB_PRESENT_CONFIGURE_NOTIFY:
		{
			TRACE_BEGIN("DRI3:PRESENT_CONFIGURE_NOTIFY");
			xcb_present_configure_notify_event_t *ce = (void *) ge;
			priv->width = ce->width;
			priv->height = ce->height;
			TRACE_END();
			break;
		}

		case XCB_PRESENT_COMPLETE_NOTIFY:
		{
			TRACE_BEGIN("DRI3:PRESENT_COMPLETE_NOTIFY");
			xcb_present_complete_notify_event_t *ce = (void *) ge;
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
			}
			else
			{
				priv->recv_msc_serial = ce->serial;
			}

			priv->ust = ce->ust;
			priv->msc = ce->msc;
			TRACE_END();
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
					TRACE_MARK("IDLE:%d",tbm_bo_export(priv->buffers[b]->tbo));
					buf->status = dri3_buffer_idle;
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
	xcb_connection_t *c;

	TPL_ASSERT(priv);
	TPL_ASSERT(priv->dpy);

	c = XGetXCBConnection(priv->dpy);

        TRACE_BEGIN("DRI3:FLUSH_PRESENT_EVENTS");
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
	TRACE_END();
}

static tpl_bool_t
dri3_wait_for_notify(xcb_connection_t *c, dri3_drawable *priv)
{
	xcb_generic_event_t *ev;
	xcb_present_generic_event_t *ge;

	TPL_ASSERT(c);
	TPL_ASSERT(priv);

	TRACE_BEGIN("TPL:DRI3:WAIT_FOR_NOTIFY");

	if (((uint32_t)priv->send_sbc) == 0)
	{
		TRACE_END();
		return TPL_TRUE;
	}
	for (;;)
	{
		if( (uint32_t)priv->send_sbc <= (uint32_t)priv->recv_sbc )
		{
			TRACE_END();
			return TPL_TRUE;
		}

		xcb_flush(c);
		ev = xcb_wait_for_special_event(c, priv->special_event);
		if (!ev)
		{
			TRACE_END();
			return TPL_FALSE;
		}
		ge = (void *) ev;
		dri3_handle_present_event(priv, ge);
	}
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

	TPL_ASSERT(c);
	TPL_ASSERT(priv);

	for (;;)
	{
		for (b = 0; b < dri3_max_back; b++)
		{
			int id = (b + priv->cur_back + 1) % dri3_max_back;
			int pre_id = (id + dri3_max_back - 2) % dri3_max_back;

			dri3_buffer *buffer = priv->buffers[id];
			dri3_buffer *pre_buffer = priv->buffers[pre_id];

			if (pre_buffer && pre_buffer->status != dri3_buffer_posted)
			pre_buffer->status = dri3_buffer_idle;

			if (!buffer || buffer->status == dri3_buffer_idle)
			{
				priv->cur_back = id;
				return id;
			}
		}

		xcb_flush(c);
		TRACE_BEGIN("DDK:DRI3:XCBWAIT");
		ev = xcb_wait_for_special_event(c, priv->special_event);
		TRACE_END();

		if (!ev)
		{
			return -1;
		}

		ge = (void *) ev;
		dri3_handle_present_event(priv, ge);
	}
}

/** dri3_alloc_render_buffer
 *
 *  allocate a render buffer and create an X pixmap from that
 *
 */
static dri3_buffer *
dri3_alloc_render_buffer(dri3_drawable *priv,
		int width, int height, int depth, int cpp)
{
	Display *dpy;
	Drawable draw;
	dri3_buffer *buffer = NULL;
	xcb_connection_t *c;
	xcb_pixmap_t pixmap = 0;
        int buffer_fd;
	int size;
	tbm_bo_handle handle;
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;

	TPL_ASSERT(priv);
	TPL_ASSERT(priv->dpy);

	dpy = priv->dpy;
	draw = priv->xDrawable;

	c = XGetXCBConnection(dpy);

	/* Allocate the image from the driver
	 */
	buffer = calloc(1, sizeof (dri3_buffer));
	if (!buffer)
	{
		TPL_ERR("Failed to allocate buffer!");
		goto no_buffer;
	}

	/* size = height * width * depth/8;*/
	/* size = ((width * 32)>>5) * 4 * height; */
	/* calculate pitch and size by input parameter cpp */
	/* buffer->pitch = width*(cpp/8); */

	/* Modify the calculation of pitch (strdie) */
	buffer->pitch = SIZE_ALIGN((width * cpp)>>3, ALIGNMENT_PITCH_ARGB);

	size = buffer->pitch*height;

	buffer->tbo = tbm_bo_alloc(priv->bufmgr, size, TBM_BO_DEFAULT);
	if (NULL == buffer->tbo)
	{
		TPL_ERR("TBM bo alloc failed!");
		free(buffer);
		goto no_buffer;
	}

	/* dup tbo, because X will close it */
	/* 2015-04-08 joonbum.ko@samsung.com */
	/* delete tbm_bo_get_handle function call and
	   add tbm_bo_export_fd function call */

	handle = tbm_bo_get_handle(buffer->tbo, TBM_DEVICE_3D);
	buffer_fd = dup(handle.u32);

	/* buffer_fd = tbm_bo_export_fd(buffer->tbo);*/
	/* 2015-04-08 joonbum.ko@samsung.com */
	/* disable the value dma_buf_fd */
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
	/* 2015-04-08 joonbum.ko@samsung.com */
	/* buffer_fd is unuseful */
	/* close(buffer_fd);*/

	if (error)
	{
		TPL_ERR("No pixmap!");
		goto no_pixmap;
	}
	if (0 == pixmap)
	{
		TPL_ERR("No pixmap!");
		goto no_pixmap;
	}

	buffer->pixmap = pixmap;
	buffer->own_pixmap = TPL_TRUE;
	buffer->width = width;
	buffer->height = height;
	buffer->flags = 0;

	return buffer;
no_pixmap:
	tbm_bo_unref(buffer->tbo);
	free(buffer);
no_buffer:
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
	xcb_connection_t *c;

	TPL_ASSERT(pdraw);
	TPL_ASSERT(buffer);
	TPL_ASSERT(pdraw->dpy);

	c = XGetXCBConnection(pdraw->dpy);

	/* 2015-04-08 joonbum.ko@samsung.com */
	/* if drawable type is pixmap, it requires only free buffer */
	if (!pdraw->is_pixmap)
	{
		if (buffer->own_pixmap)
			xcb_free_pixmap(c, buffer->pixmap);
		tbm_bo_unref(buffer->tbo);
		/* added a ref when created and unref while free, see dri3_get_pixmap_buffer */
	}

	buffer = NULL;
}


/** dri3_get_window_buffer
 *
 * Find a front or back buffer, allocating new ones as necessary
 */

/* 2015-04-08 joonbum.ko@samsung.com */
/* Change the value of old_dma_fd to old_bo_name */
static dri3_buffer *
dri3_get_window_buffer(void *loaderPrivate, int cpp)
{
	dri3_drawable		*priv = loaderPrivate;
	xcb_connection_t	*c;
	dri3_buffer		*backbuffer = NULL;
	int			back_buf_id,reuse = 1;
	uint32_t                old_bo_name = 0;

	TPL_ASSERT(priv);
	TPL_ASSERT(priv->dpy);

	c = XGetXCBConnection(priv->dpy);

	TRACE_BEGIN("DDK:DRI3:GETBUFFERS:WINDOW");
	TRACE_BEGIN("DDK:DRI3:FINDBACK");
	back_buf_id = dri3_find_back(c, priv);
	TRACE_END();

	backbuffer = priv->buffers[back_buf_id];

	/* Allocate a new buffer if there isn't an old one, or if that
	 * old one is the wrong size.
	 */
	if (!backbuffer || backbuffer->width != priv->width ||
			backbuffer->height != priv->height )
	{
		dri3_buffer   *new_buffer;

		/* Allocate the new buffers
		 */
		TRACE_BEGIN("DDK:DRI3:ALLOCRENDERBUFFER");
		new_buffer = dri3_alloc_render_buffer(priv,
				priv->width, priv->height, priv->depth, cpp);
		TRACE_END();

		if (!new_buffer)
		{
			TRACE_END();
			return NULL;
		}
		if (backbuffer)
		{
			/* [BEGIN: 20141125-xuelian.bai] Size not match,this buffer
			 * must be removed from buffer cache, so we have to save
			 * dma_buf_fd of old buffer.*/
			old_bo_name = tbm_bo_export(backbuffer->tbo);
			/* [END: 20141125-xuelian.bai] */
			TRACE_BEGIN("DDK:DRI3:FREERENDERBUFFER");
			dri3_free_render_buffer(priv, backbuffer);
			TRACE_END();
		}
		backbuffer = new_buffer;
		backbuffer->buffer_type = dri3_buffer_back;
		backbuffer->old_bo_name = old_bo_name;
		priv->buffers[back_buf_id] = backbuffer;
		reuse = 0;
	}

	backbuffer->flags = DRI2_BUFFER_FB;
	backbuffer->status = dri3_buffer_busy;
	if(reuse)
	{
		backbuffer->flags |= DRI2_BUFFER_REUSED;
	}
	/* Return the requested buffer */
	TRACE_END();

	TRACE_MARK("%d",tbm_bo_export(backbuffer->tbo));

	return backbuffer;
}

/* 2015-04-07  joonbum.ko@samsung.com */
/* modify internal flow of dri3_get_pixmap_buffer */
/* add 3rd argument for stride information */
static dri3_buffer *
dri3_get_pixmap_buffer(void *loaderPrivate, Pixmap pixmap, int cpp)/*TODO:format*/
{
	dri3_drawable *pdraw = loaderPrivate;
	dri3_buffer *buffer = NULL;
	xcb_dri3_buffer_from_pixmap_cookie_t bp_cookie;
	xcb_dri3_buffer_from_pixmap_reply_t  *bp_reply;
	int *fds;
	Display *dpy;
	xcb_connection_t *c;
	tbm_bo tbo = NULL;

	TPL_ASSERT(pdraw);
	TPL_ASSERT(pdraw->dpy);

	TRACE_BEGIN("DDK:DRI3:GETBUFFERS:PIXMAP");

	dpy = pdraw->dpy;
	c = XGetXCBConnection(dpy);

	/* Get an FD for the pixmap object
	 */
	bp_cookie = xcb_dri3_buffer_from_pixmap(c, pixmap);
	bp_reply = xcb_dri3_buffer_from_pixmap_reply(c, bp_cookie, NULL);
	if (!bp_reply)
	{
		goto no_image;
	}
	fds = xcb_dri3_buffer_from_pixmap_reply_fds(c, bp_reply);

	tbo = tbm_bo_import_fd(pdraw->bufmgr,(tbm_fd)(*fds));

	if (!buffer)
	{
		buffer = calloc(1, sizeof (dri3_buffer));
		if (!buffer)
			goto no_buffer;
	}

	buffer->tbo = tbo;
	/* 2015-04-08 joonbum.ko@samsung.com */
	/* disable the value dma_buf_fd */
	buffer->dma_buf_fd = *fds;
	buffer->pixmap = pixmap;
	buffer->own_pixmap = TPL_FALSE;
	buffer->width = bp_reply->width;
	buffer->height = bp_reply->height;
	buffer->buffer_type = dri3_buffer_front;
	buffer->flags = DRI3_BUFFER_REUSED;
	/* 2015-04-07 joonbum.ko@samsung.com */
	/* add buffer information(cpp, pitch, size) */
	buffer->cpp = cpp;
	buffer->pitch = bp_reply->stride;
	buffer->size = buffer->pitch * bp_reply->height;

	pdraw->buffers[dri3_max_back] = buffer;

	/* 2015-04-08 joonbum.ko@samsung.com */
	/* fds is unuseful */
	close(*fds);
	TRACE_END();
	return buffer;

/* 2015-04-09 joonbum.ko@samsung.com */
/* change the lable order */
no_image:
	if (buffer)
		free(buffer);
no_buffer:
	TRACE_END();
	return NULL;
}

static dri3_buffer *dri3_get_buffers(XID drawable,  void *loaderPrivate,
		unsigned int *attachments, int cpp)
{
	dri3_drawable *priv = loaderPrivate;
	dri3_buffer *buffer = NULL;

	TPL_ASSERT(priv);
	TPL_ASSERT(attachments);

	TRACE_BEGIN("DDK:DRI3:GETBUFFERS");

	if (drawable != priv->xDrawable)
	{
		TPL_ERR("Drawable mismatch!");
		TRACE_END();
		return NULL;
	}

	if (!dri3_update_drawable(loaderPrivate))
	{
		TPL_ERR("Update drawable failed!");
		TRACE_END();
		return NULL;
	}

	if (*attachments == dri3_buffer_front)
		buffer = dri3_get_pixmap_buffer(loaderPrivate,
				priv->xDrawable, cpp);
	else
		buffer = dri3_get_window_buffer(loaderPrivate, cpp);

	if (NULL == buffer)
	{
		TPL_ERR("Get buffer failed!");
		return NULL;
	}

	TRACE_END();

	return buffer;
}

/******************************************************
 * dri3_swap_buffers
 * swap back buffer with front buffer
 * Make the current back buffer visible using the present extension
 * if (region_t==0),swap whole frame, else swap with region
 ******************************************************/
static int64_t
dri3_swap_buffers(Display *dpy, void *priv, tpl_buffer_t *frame_buffer, int interval, XID region_t)
{

	int64_t		ret = -1;
	int64_t		target_msc = 0;
	int64_t		divisor = 0;
	int64_t		remainder = 0;
	xcb_connection_t *c;
	dri3_drawable	*pDrawable;
	dri3_buffer	*back = NULL;
        int		i = 0;

	TPL_ASSERT(dpy);
	TPL_ASSERT(priv);
	TPL_ASSERT(frame_buffer);

	c = XGetXCBConnection(dpy);

	pDrawable = (dri3_drawable*) priv;
	back = (dri3_buffer*) frame_buffer->backend.data;

	if ((back == NULL)||(pDrawable == NULL)||(pDrawable->is_pixmap != 0))
	{
		TRACE_END();
		return ret;
	}

	/* Process any present events that have been received from the X
	 * server until receive complete notify.
	 */
	if (!dri3_wait_for_notify(c, pDrawable))
	{
		TRACE_END();
		return ret;
	}
	 /* [BEGIN: 20140119-leiba.sun] Add support for buffer age
	  * When swap buffer, increase buffer age of every back buffer */
	for(i = 0; i < dri3_max_back; i++)
	{
		if((pDrawable->buffers[i] != NULL)&&(pDrawable->buffers[i]->buffer_age > 0))
			pDrawable->buffers[i]->buffer_age++;
	}
	back->buffer_age = 1;
	/* [END:20150119-leiba.sun] */
	/* set busy flag */
	back->status = dri3_buffer_posted;

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

	back->last_swap = pDrawable->send_sbc;

	TRACE_MARK("SWAP:%d", tbm_bo_export(back->tbo)) ;
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
			0,
			XCB_PRESENT_OPTION_NONE,
			/*target_msc*/0,
			divisor,
			remainder, 0, NULL);

	ret = (int64_t) pDrawable->send_sbc;

	xcb_flush(c);

	++(pDrawable->stamp);

	return ret;
}

tpl_bool_t
__tpl_x11_dri3_buffer_init(tpl_buffer_t *buffer)
{
	TPL_IGNORE(buffer);
	return TPL_TRUE;
}

void
__tpl_x11_dri3_buffer_fini(tpl_buffer_t *buffer)
{
	dri3_buffer* back;

	TPL_ASSERT(buffer);

	back = (dri3_buffer*)buffer->backend.data;

	if (back)
	{
		tbm_bo bo = back->tbo;
		tbm_bo_map(bo, TBM_DEVICE_3D, TBM_OPTION_READ);
		tbm_bo_unmap(bo);
		tbm_bo_unref(bo);
		buffer->backend.data = NULL;
		free(back);
	}
}

void *
__tpl_x11_dri3_buffer_map(tpl_buffer_t *buffer, int size)
{
	tbm_bo bo;
	tbm_bo_handle handle;

	TPL_ASSERT(buffer);

	TPL_IGNORE(size);
	bo = ((dri3_buffer*)buffer->backend.data)->tbo;
	TPL_ASSERT(bo);

	handle = tbm_bo_get_handle(bo, TBM_DEVICE_CPU);
	return handle.ptr;
}

void
__tpl_x11_dri3_buffer_unmap(tpl_buffer_t *buffer, void *ptr, int size)
{
	TPL_IGNORE(buffer);
	TPL_IGNORE(ptr);
	TPL_IGNORE(size);

	/* Do nothing. */
}

tpl_bool_t
__tpl_x11_dri3_buffer_lock(tpl_buffer_t *buffer, tpl_lock_usage_t usage)
{
	tbm_bo          bo;
	tbm_bo_handle   handle;
	dri3_buffer*    back;

	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.data);

	back = (dri3_buffer*) buffer->backend.data;
	bo = back->tbo;

	if (NULL == bo)
	{
		TPL_ERR("bo is NULL!");
		return TPL_FALSE;
	}

	TRACE_BEGIN("TPL:BUFFERLOCK:%d",tbm_bo_export(bo));

	TPL_OBJECT_UNLOCK(buffer);

	switch (usage)
	{
		case TPL_LOCK_USAGE_GPU_READ:
			handle = tbm_bo_map(bo, TBM_DEVICE_3D, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_GPU_WRITE:
			handle = tbm_bo_map(bo, TBM_DEVICE_3D, TBM_OPTION_WRITE);
			break;
		case TPL_LOCK_USAGE_CPU_READ:
			handle = tbm_bo_map(bo, TBM_DEVICE_CPU, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_CPU_WRITE:
			handle = tbm_bo_map(bo, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
			break;
		default:
			TPL_ASSERT(TPL_FALSE);
			return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(buffer);

	if (handle.u32 != 0 || handle.ptr != NULL)
	{
		TRACE_END();
		return TPL_FALSE;
	}
	TRACE_END();
	return TPL_TRUE;
}

void
__tpl_x11_dri3_buffer_unlock(tpl_buffer_t *buffer)
{
	dri3_buffer *back;
	tbm_bo bo;

	TPL_ASSERT(buffer);

	back = (dri3_buffer*) buffer->backend.data;
	bo = back->tbo;

	if (NULL == bo)
	{
		TPL_ERR("bo is NULL!");
		return;
	}

	TRACE_BEGIN("TPL:BUFFERUNLOCK:%d",tbm_bo_export(back->tbo));

	TPL_OBJECT_UNLOCK(buffer);
	tbm_bo_unmap(bo);
	TPL_OBJECT_LOCK(buffer);

	TRACE_END();
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

	TPL_ASSERT(display);

        XInitThreads();
	if (display->native_handle == NULL)
	{
		display->native_handle = XOpenDisplay(NULL);
		TPL_ASSERT(display->native_handle != NULL);
	}

	pthread_mutex_lock(&mutex);

	if (global.display_count == 0)
	{
		tpl_bool_t      xres = TPL_FALSE;
		Window          root = 0;
		drm_magic_t     magic;

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

		__tpl_list_init(&dri3_drawable_list);

		/* [BEGIN: 20141125-xuelian.bai] Add env for setting number of back buffers*/
		{
			const char *backend_env = NULL;
			int count = 0;
			backend_env = getenv("MALI_EGL_DRI3_BUF_NUM");
                        /* 2015-05-13 joonbum.ko@samsung.com */
                        /* Change the value of dri3_max_back 5 to 3 */
			if (!backend_env || strlen(backend_env) == 0)
				dri3_max_back  = 3; /* Default value is 3*/
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

		__tpl_list_fini(&dri3_drawable_list, NULL);
	}

	pthread_mutex_unlock(&mutex);

}

static tpl_bool_t
__tpl_x11_dri3_surface_init(tpl_surface_t *surface)
{
	Display *display = NULL;
	XID drawable;
	tpl_x11_dri3_surface_t *x11_surface;

	TPL_ASSERT(surface);

	x11_surface = (tpl_x11_dri3_surface_t *)calloc(1, sizeof(tpl_x11_dri3_surface_t));
	if (x11_surface == NULL)
	{
		TPL_ERR("Failed to allocate buffer!");
		return TPL_FALSE;
	}

	x11_surface->latest_post_interval = -1;
	__tpl_list_init(&x11_surface->buffer_cache);

	display = (Display *)surface->display->native_handle;
	drawable = (XID)surface->native_handle;

	x11_surface->drawable = dri3_create_drawable(display, drawable);

	surface->backend.data = (void *)x11_surface;
	if (surface->type == TPL_SURFACE_TYPE_WINDOW)
	{
		__tpl_x11_display_get_window_info(surface->display,
				surface->native_handle,
				&surface->width, &surface->height, NULL, 0, 0);
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
	tpl_x11_dri3_surface_t  *x11_surface;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);

	display = (Display *) surface->display->native_handle;
	x11_surface = (tpl_x11_dri3_surface_t *) surface->backend.data;

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
__tpl_x11_dri3_surface_post_internal(tpl_surface_t *surface,
                tpl_frame_t *frame,
		tpl_bool_t is_worker)
{
	Display         *display = NULL;
	tpl_x11_dri3_surface_t *x11_surface;
	XRectangle      *xrects;
	XRectangle      xrects_stack[TPL_STACK_XRECTANGLE_SIZE];

	TPL_ASSERT(surface);
	TPL_ASSERT(frame);

	TRACE_BEGIN("DDK:DRI3:SWAPBUFFERS");
	x11_surface = (tpl_x11_dri3_surface_t *)surface->backend.data;

	display = __tpl_x11_dri3_get_worker_display();

	if (frame->interval != x11_surface->latest_post_interval)
	{
		x11_surface->latest_post_interval = frame->interval;/*FIXME:set interval?*/
	}

	if (__tpl_region_is_empty(&frame->damage))
	{
		dri3_swap_buffers(display, x11_surface->drawable, frame->buffer, 0,0);
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

		dri3_swap_buffers(display, x11_surface->drawable, frame->buffer, 0,
				x11_surface->damage);
	}
	frame->state = TPL_FRAME_STATE_POSTED;

	TRACE_END();
}

static void
__tpl_x11_dri3_surface_post(tpl_surface_t *surface, tpl_frame_t *frame)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(frame);

	__tpl_x11_dri3_surface_post_internal(surface, frame, TPL_TRUE);
}

static tpl_bool_t
__tpl_x11_dri3_surface_begin_frame(tpl_surface_t *surface)
{
	tpl_frame_t *prev_frame;

	TPL_ASSERT(surface);

	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return TPL_TRUE;

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

	return TPL_TRUE;
}

static tpl_bool_t
__tpl_x11_dri3_surface_validate_frame(tpl_surface_t *surface)
{
	tpl_frame_t *prev_frame;
	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return TPL_TRUE;

	if (surface->frame == NULL)
		return TPL_TRUE;

	prev_frame = __tpl_surface_get_latest_frame(surface);

	if (prev_frame && prev_frame->state != TPL_FRAME_STATE_POSTED)
	{
		if ((DRI2_BUFFER_IS_FB(prev_frame->buffer->backend.flags) &&
			global.fb_swap_type == TPL_X11_SWAP_TYPE_LAZY) ||
			(!DRI2_BUFFER_IS_FB(prev_frame->buffer->backend.flags) &&
			global.win_swap_type == TPL_X11_SWAP_TYPE_LAZY))
		{
			__tpl_surface_wait_all_frames(surface);
			return TPL_TRUE;
		}
	}
	return TPL_TRUE;
}

static tpl_bool_t
__tpl_x11_dri3_surface_end_frame(tpl_surface_t *surface)
{
	tpl_frame_t *frame;
	tpl_x11_dri3_surface_t *x11_surface;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->backend.data);

	frame = __tpl_surface_get_latest_frame(surface);
	x11_surface = (tpl_x11_dri3_surface_t *) surface->backend.data;

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

	return TPL_TRUE;
}

/* 2015-04-08 joonbum.ko@samsung.com */
/* change the key value of tpl_buffer_t from dma_buf_fd to tbo name */
static tpl_buffer_t *
__tpl_x11_dri3_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	Drawable drawable;
	dri3_buffer *buffer = NULL;
	tpl_buffer_t *tpl_buffer = NULL;
	uint32_t attachments[1] = { dri3_buffer_back };
	tbm_bo bo;
	tbm_bo_handle bo_handle;
	tpl_x11_dri3_surface_t *x11_surface;
	int cpp = 0;

	TPL_ASSERT(surface);

	x11_surface = (tpl_x11_dri3_surface_t *)surface->backend.data;

	if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
	{
		attachments[0] = dri3_buffer_front;
	}

	drawable = (Drawable)surface->native_handle;

	/* [BEGIN: 20141125-xing.huang] Get the current buffer via DRI3. */
	cpp = 32;/*_mali_surface_specifier_bpp(&(surface->sformat)); cpp get from mali is not right */
	/* [END: 20141125-xing.huang] */

	buffer = dri3_get_buffers(drawable, x11_surface->drawable, attachments, cpp);

	if (DRI2_BUFFER_IS_REUSED(buffer->flags))
	{
		tpl_buffer = __tpl_x11_surface_buffer_cache_find(
				&x11_surface->buffer_cache,
				tbm_bo_export(buffer->tbo));

		if (tpl_buffer)
		{
			/* If the buffer name is reused and there's a cache
			 * entry for that name, just update the buffer age
			 * and return. */
			/* [BEGIN: 20140119-leiba.sun] Add support for buffer age */
			tpl_buffer->age = buffer->buffer_age;
			/* [END:20150119-leiba.sun] */

			if (surface->type == TPL_SURFACE_TYPE_PIXMAP)
				tbm_bo_unref (buffer->tbo);

			goto done;
		}
	}

	if (!tpl_buffer)
	{
		/* Remove the buffer from the cache. */
                __tpl_x11_surface_buffer_cache_remove(
				&x11_surface->buffer_cache,
				tbm_bo_export(buffer->tbo));
		if(buffer->old_bo_name != 0)
		{
			__tpl_x11_surface_buffer_cache_remove(
					&x11_surface->buffer_cache,
					buffer->old_bo_name);
			buffer->old_bo_name = 0;
		}
	}

	bo = buffer->tbo;

	if (bo == NULL)
	{
		TPL_ERR("bo is NULL!");
		goto done;
	}

	bo_handle = tbm_bo_get_handle(bo, TBM_DEVICE_3D);

	/* Create tpl buffer. */
	tpl_buffer = __tpl_buffer_alloc(surface, (size_t) tbm_bo_export(buffer->tbo),
			(int)bo_handle.u32,
			buffer->width, buffer->height, buffer->cpp * 8, buffer->pitch);
	if (NULL == tpl_buffer)
	{
		TPL_ERR("TPL buffer alloc failed!");
		goto done;
	}

	if (surface->type != TPL_SURFACE_TYPE_PIXMAP)
		tbm_bo_ref(buffer->tbo);

	tpl_buffer->age = DRI2_BUFFER_GET_AGE(buffer->flags);
	tpl_buffer->backend.data = (void *)buffer;
	tpl_buffer->backend.flags = buffer->flags;
	/* [BEGIN: 20140119-leiba.sun] Add support for buffer age
	 * save surface for later use */
	tpl_buffer->surface = surface;
	/* [END:20150119-leiba.sun] */

	__tpl_x11_surface_buffer_cache_add(&x11_surface->buffer_cache, tpl_buffer);
	tpl_object_unreference(&tpl_buffer->base);
done:
	if (reset_buffers)
	{
		/* Users use this output value to check if they have to reset previous buffers. */
		*reset_buffers = !DRI2_BUFFER_IS_REUSED(buffer->flags) ||
			buffer->width != surface->width || buffer->height != surface->height;
	}

	return tpl_buffer;
}

/* [BEGIN: 20140119-leiba.sun] Add support for buffer age */
int
__tpl_x11_dri3_get_buffer_age(tpl_buffer_t *buffer)
{
        dri3_buffer *back;

	TPL_ASSERT(buffer);

        back = (dri3_buffer*) buffer->backend.data;

        TPL_ASSERT(back);

        return back->buffer_age;
}
/* [END:20150119-leiba.sun] */


tpl_bool_t
__tpl_display_choose_backend_x11_dri3(tpl_handle_t native_dpy)
{
	TPL_IGNORE(native_dpy);
	/* X11 display accepts any type of handle. So other backends must be choosen before this. */
	return TPL_TRUE;
}

void
__tpl_display_init_backend_x11_dri3(tpl_display_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_X11_DRI3;
	backend->data = NULL;

	backend->init			= __tpl_x11_dri3_display_init;
	backend->fini			= __tpl_x11_dri3_display_fini;
	backend->query_config		= __tpl_x11_display_query_config;
	backend->filter_config		= NULL;
	backend->get_window_info		= __tpl_x11_display_get_window_info;
	backend->get_pixmap_info		= __tpl_x11_display_get_pixmap_info;
	backend->flush			= __tpl_x11_display_flush;
}

void
__tpl_surface_init_backend_x11_dri3(tpl_surface_backend_t *backend)
{
	TPL_ASSERT(backend);

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
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_X11_DRI3;
	backend->data = NULL;

	backend->init		= __tpl_x11_dri3_buffer_init;
	backend->fini		= __tpl_x11_dri3_buffer_fini;
	backend->map		= __tpl_x11_dri3_buffer_map;
	backend->unmap		= __tpl_x11_dri3_buffer_unmap;
	backend->lock		= __tpl_x11_dri3_buffer_lock;
	backend->unlock		= __tpl_x11_dri3_buffer_unlock;
        /* [BEGIN: 20140119-leiba.sun] Add support for buffer age */
        backend->get_buffer_age	= __tpl_x11_dri3_get_buffer_age;
        /* [END:20150119-leiba.sun] */
}
