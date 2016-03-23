#define inline __inline__

#include <wayland-client.h>

#include "wayland-egl/wayland-egl-priv.h"

#include <drm.h>
#include <xf86drm.h>

#undef inline

#include "tpl_internal.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <poll.h>

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tbm_surface_queue.h>
#include <wayland-tbm-client.h>
#include <wayland-tbm-server.h>

/* In wayland, application and compositor create its own drawing buffers. Recommend size is more than 2. */
#define CLIENT_QUEUE_SIZE 3

typedef struct _tpl_wayland_display tpl_wayland_display_t;
typedef struct _tpl_wayland_surface tpl_wayland_surface_t;
typedef struct _tpl_wayland_buffer tpl_wayland_buffer_t;

struct _tpl_wayland_display {
	tbm_bufmgr bufmgr;
	struct wayland_tbm_client *wl_tbm_client;
	struct wl_event_queue *wl_queue;
	struct wl_registry *wl_registry;
};

struct _tpl_wayland_surface {
	tbm_surface_queue_h tbm_queue;
	tbm_surface_h current_buffer;
	tpl_bool_t resized;

	/* I'm not fully understood current_buffer use-case,
	 * so just to test I've added new field which represent
	 * whether we have some buffer under rendering or not */
	int rendering_is;

	/* it's buffer which we will post after surface_enter event has been arrived
	 * it's content, if exist, is rendered with gles during surface_leave event
	 * handling, so we've prevented it to be posted at that time and will be able to
	 * post it after surface_enter event will have been arrived -- a.k.a
	 * immediately app start effect :-) */
	tbm_surface_h delayed_buffer;

	/* id of thread which is responsible for tpl_wayland queue's dispatching */
	pthread_t surface_enter_leave_events_thread_id;

	pthread_mutex_t mtx;
	pthread_cond_t surface_is_undeground;
};

struct _tpl_wayland_buffer {
	tpl_display_t *display;
	tbm_bo bo;
	tpl_wayland_surface_t *wayland_surface;
	struct wl_proxy *wl_proxy;
};

static const struct wl_registry_listener registry_listener;
static const struct wl_callback_listener sync_listener;
static const struct wl_callback_listener frame_listener;
static const struct wl_buffer_listener buffer_release_listener;

#define TPL_BUFFER_CACHE_MAX_ENTRIES 40

static int tpl_wayland_buffer_key;
#define KEY_TPL_WAYLAND_BUFFER  (unsigned long)(&tpl_wayland_buffer_key)

static void __tpl_wayland_buffer_free(tpl_wayland_buffer_t *wayland_buffer);

static TPL_INLINE tpl_wayland_buffer_t *
__tpl_wayland_get_wayland_buffer_from_tbm_surface(tbm_surface_h surface)
{
	tbm_bo bo;
	tpl_wayland_buffer_t *buf = NULL;

	bo = tbm_surface_internal_get_bo(surface, 0);
	tbm_bo_get_user_data(bo, KEY_TPL_WAYLAND_BUFFER, (void **)&buf);

	return buf;
}

static TPL_INLINE void
__tpl_wayland_set_wayland_buffer_to_tbm_surface(tbm_surface_h surface,
		tpl_wayland_buffer_t *buf)
{
	tbm_bo bo;

	bo = tbm_surface_internal_get_bo(surface, 0);

	tbm_bo_add_user_data(bo, KEY_TPL_WAYLAND_BUFFER,
			     (tbm_data_free)__tpl_wayland_buffer_free);

	tbm_bo_set_user_data(bo, KEY_TPL_WAYLAND_BUFFER, buf);
}

static TPL_INLINE tpl_bool_t
__tpl_wayland_display_is_wl_display(tpl_handle_t native_dpy)
{
	TPL_ASSERT(native_dpy);

	struct wl_interface *wl_egl_native_dpy = *(void **) native_dpy;

	/* MAGIC CHECK: A native display handle is a wl_display if the de-referenced first value
	   is a memory address pointing the structure of wl_display_interface. */
	if ( wl_egl_native_dpy == &wl_display_interface ) {
		return TPL_TRUE;
	}

	if (strncmp(wl_egl_native_dpy->name, wl_display_interface.name,
		    strlen(wl_display_interface.name)) == 0) {
		return TPL_TRUE;
	}

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

	wl_proxy_set_queue((struct wl_proxy *) callback, wayland_display->wl_queue);

	while (ret != -1 && !done) {
		ret = wl_display_dispatch_queue(wl_dpy, wayland_display->wl_queue);
	}

	return ret;
}

static tpl_result_t
__tpl_wayland_display_init(tpl_display_t *display)
{
	tpl_wayland_display_t *wayland_display = NULL;

	TPL_ASSERT(display);

	/* Do not allow default display in wayland. */
	if (!display->native_handle) {
		TPL_ERR("Invalid native handle for display.");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	wayland_display = (tpl_wayland_display_t *) calloc(1,
			  sizeof(tpl_wayland_display_t));
	if (!wayland_display) {
		TPL_ERR("Failed to allocate memory for new tpl_wayland_display_t.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	display->backend.data = wayland_display;
	display->bufmgr_fd = -1;

	if (__tpl_wayland_display_is_wl_display(display->native_handle)) {
		struct wl_display *wl_dpy =
			(struct wl_display *)display->native_handle;
		wayland_display->wl_tbm_client =
			wayland_tbm_client_init((struct wl_display *) wl_dpy);

		if (!wayland_display->wl_tbm_client) {
			TPL_ERR("Wayland TBM initialization failed!");
			goto free_wl_display;
		}

		wayland_display->wl_queue = wl_display_create_queue(wl_dpy);
		if (!wayland_display->wl_queue) {
			TPL_ERR("Failed to create wl_queue with wl_dpy(%p).", wl_dpy);
			goto free_wl_display;
		}

		wayland_display->wl_registry = wl_display_get_registry(wl_dpy);
		if (!wayland_display->wl_registry) {
			TPL_ERR("Failed to get wl_registry with wl_dpy(%p).", wl_dpy);
			goto destroy_queue;
		}

		wl_proxy_set_queue((struct wl_proxy *)wayland_display->wl_registry,
				   wayland_display->wl_queue);
	} else {
		goto free_wl_display;
	}

	return TPL_ERROR_NONE;

destroy_queue:
	wl_event_queue_destroy(wayland_display->wl_queue);

free_wl_display:
	if (wayland_display) {
		free(wayland_display);
		display->backend.data = NULL;
	}
	return TPL_ERROR_INVALID_OPERATION;
}

static void
__tpl_wayland_display_fini(tpl_display_t *display)
{
	tpl_wayland_display_t *wayland_display;

	TPL_ASSERT(display);

	wayland_display = (tpl_wayland_display_t *)display->backend.data;
	if (wayland_display) {
		wayland_tbm_client_deinit(wayland_display->wl_tbm_client);
		free(wayland_display);
	}
	display->backend.data = NULL;
}

static tpl_result_t
__tpl_wayland_display_query_config(tpl_display_t *display,
				   tpl_surface_type_t surface_type,
				   int red_size, int green_size,
				   int blue_size, int alpha_size,
				   int color_depth, int *native_visual_id,
				   tpl_bool_t *is_slow)
{
	TPL_ASSERT(display);

	if (surface_type == TPL_SURFACE_TYPE_WINDOW && red_size == 8 &&
	    green_size == 8 && blue_size == 8 &&
	    (color_depth == 32 || color_depth == 24)) {

		if (alpha_size == 8) {
			if (native_visual_id) *native_visual_id = TBM_FORMAT_ARGB8888;
			if (is_slow) *is_slow = TPL_FALSE;
			return TPL_ERROR_NONE;
		}
		if (alpha_size == 0) {
			if (native_visual_id) *native_visual_id = TBM_FORMAT_XRGB8888;
			if (is_slow) *is_slow = TPL_FALSE;
			return TPL_ERROR_NONE;
		}
	}

	return TPL_ERROR_INVALID_PARAMETER;
}

static tpl_result_t
__tpl_wayland_display_filter_config(tpl_display_t *display, int *visual_id,
				    int alpha_size)
{
	TPL_IGNORE(display);
	TPL_IGNORE(visual_id);
	TPL_IGNORE(alpha_size);
	return TPL_ERROR_NONE;
}

static tpl_result_t
__tpl_wayland_display_get_window_info(tpl_display_t *display,
				      tpl_handle_t window, int *width,
				      int *height, tbm_format *format,
				      int depth, int a_size)
{
	TPL_ASSERT(display);
	TPL_ASSERT(window);

	struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)window;

	if (format) {
		/* Wayland-egl window doesn't have native format information.
		   It is fixed from 'EGLconfig' when called eglCreateWindowSurface().
		   So we use the tpl_surface format instead. */
		tpl_surface_t *surface = wl_egl_window->private;
		if (surface) *format = surface->format;
		else {
			if (a_size == 8) *format = TBM_FORMAT_ARGB8888;
			else if (a_size == 0) *format = TBM_FORMAT_XRGB8888;
		}
	}
	if (width != NULL) *width = wl_egl_window->width;
	if (height != NULL) *height = wl_egl_window->height;

	return TPL_ERROR_NONE;
}

static void
__cb_client_window_resize_callback(struct wl_egl_window *wl_egl_window,
				   void *private);

//-------------------------- TWS-1456 -------------------------------------------------

/* this is dummy implementation of enqueue_buffer which is used to avoid ordinary way of
 * buffer posting -- we need to prevent buffer to be posted and need to block gles thread
 * until surface_enter event will have been delivered */
static tpl_result_t
__tpl_wayland_dummy_enqueue_buffer(tpl_surface_t *surface,
	     tbm_surface_h tbm_surface,
	     int num_rects, const int *rects)
{
	tpl_result_t res = TPL_ERROR_NONE;

	//pthread_cond_wait()
	return res;
}

static tbm_surface_h
__tpl_wayland_dummy_dequeue_buffer(tpl_surface_t *surface)
{
	tbm_surface_h tbm_surface;

	//pthread_cond_wait();

	return tbm_surface;
}

/* pick up all not-dequeued and not-acquired tbm_surfaces from surface_queue (free_queue)
 * also inform about this wayland server to allow it to does actually memory release */
static void
__tpl_wayland_pick_up_surface_free_queue_buffers(tpl_surface_t *tpl_surface)
{
	tpl_wayland_display_t *wayland_display =
			(tpl_wayland_display_t *) tpl_surface->display->backend.data;
	tpl_wayland_surface_t *wayland_surface =
			(tpl_wayland_surface_t *) tpl_surface->backend.data;
	tpl_wayland_buffer_t *wayland_buffer;

	tbm_surface_h tbm_surface;

	/* iterate over available, in free_queue, tbm_surfaces */
	while (tbm_surface_queue_can_dequeue(wayland_surface->tbm_queue, 0) == 1) {

		/* dequeue from tbm_surface, so we `remove` it from free_queue, but not
		 * from list of all tbm_surfaces */
		tbm_surface_queue_dequeue(wayland_surface->tbm_queue, &tbm_surface);

		/* inform wayland server we'll destroy tmb_surfaces (on client side) soon, so it can
		   destroy itself representation of these tbm_surfaces to actually free kernel
		   'graphical' memory. */
		wayland_buffer = __tpl_wayland_get_wayland_buffer_from_tbm_surface(tbm_surface);
		wayland_tbm_client_destroy_buffer(wayland_display->wl_tbm_client,
				wayland_buffer->wl_proxy);
		tbm_surface_destroy(tbm_surface);
	}

	/* Now we cann't reset surface_queue because it has links to buffers which aren't in
	 * free_queue, we will do it later */
}

/* handler of surface_enter event, which is issued by wayland server: wl_surface_send_enter */
static void
__surface_enter_event(void *data,
		struct wl_surface *wl_surface,
		struct wl_output *output)
{
	TPL_LOG(3, "surface enter event -- wl_surface: %p.\n", wl_surface);
}

/* handler of surface_leave event, which is issued by wayland server: wl_surface_send_leave */
static void
__surface_leave_event(void *data,
	      struct wl_surface *wl_surface,
	      struct wl_output *output)
{
	tpl_surface_t *surface = (tpl_surface_t *)data;
	tpl_wayland_display_t *wayland_display =
		(tpl_wayland_display_t *) surface->display->backend.data;
	tpl_wayland_surface_t *wayland_surface =
			(tpl_wayland_surface_t *) surface->backend.data;

	TPL_LOG(3, "surface leave event -- wl_surface: %p.\n", wl_surface);

	/* make some logic to prevent rendered buffer to be posted and new buffer to be dequeued,
	 * from other thread(s) */

	TPL_OBJECT_LOCK(surface);
	if (wayland_surface->rendering_is)
	{
		/* Maybe it's paranoia, but now, I think this approach is better than
		 * insert logic directly inside
		 * __tpl_wayland_surface_enqueue_buffer/__tpl_wayland_surface_dequeue_buffer functions... */
		surface->backend.enqueue_buffer = __tpl_wayland_dummy_enqueue_buffer;
		surface->backend.dequeue_buffer = __tpl_wayland_dummy_dequeue_buffer;
	}
	TPL_OBJECT_UNLOCK(surface);

	/* Dequeue buffers from surface_queue.free_queue */

	__tpl_wayland_pick_up_surface_free_queue_buffers(surface);

	/* Wait for buffers which were posted to wayland server to release them too */

	TPL_OBJECT_UNLOCK(surface);
	while (tbm_surface_queue_can_dequeue(wayland_surface->tbm_queue, 0) == 0) {
		/* Application sent all buffers to the server. Wait for server response. */
		if (wl_display_dispatch_queue(surface->display->native_handle,
						  wayland_display->wl_queue) == -1) {
			TPL_OBJECT_LOCK(surface);
			TPL_ERR("Error during tpl wl_queue dispatching.");
			return;
		}
	}
	TPL_OBJECT_LOCK(surface);

	/* we've dequeued all tbm_surfaces from free_queue but we haven't done
	 *  real tbm_surface_unref/tbm_surface_destroy calls, so we will do it by reseting queue
	 *  Note: we have links to them even after we've dequeued them. */

	int width, height, format;

	width = tbm_surface_queue_get_width(wayland_surface->tbm_queue);
	height = tbm_surface_queue_get_height(wayland_surface->tbm_queue);
	format = tbm_surface_queue_get_format(wayland_surface->tbm_queue);

	/* it's temporary hack, due to the fact I have no access to
	   tbm_surface_queue_h internal and tbm_surface_queue_reset
	   function arguments checking; in future, I think, tbm_surface_queue_reset
	   arguments checking logic must be reviewed or something similar...*/

	/* like as tbm_queue->width++ */
	int *temp;

	temp = (int*)wayland_surface->tbm_queue;
	(*temp)++;

	/* reset surface_queue, it means unref/destroy all not-dequeued tbm_surfaces */
	tbm_surface_queue_reset(wayland_surface->tbm_queue, width, height, format);
}

static const struct wl_surface_listener wl_surface_listener =
{
	__surface_enter_event,
	__surface_leave_event
};


/* we need new thread to dispatch wayland queue @wayland_display->wl_queue we've attached
 * wl_surface object in to be able to monitor surface_leave/surface_enter events.
 * we can't do it in __tpl_wayland_surface_dequeue_buffer because, in normal case, we
 * always have buffer to dequeue (tbm_surface_queue isn't empty, so we will not dispatch
 * wayland queue and it means we cann't react to surface_enter/surface_leave events */
static void*
__tpl_wayland_surface_enter_leave_events_thread(void *arg)
{
	tpl_surface_t *surface = (tpl_surface_t *)arg;
	tpl_display_t *tpl_display = (tpl_display_t *) surface->display;
	tpl_wayland_display_t *wayland_display =
		(tpl_wayland_display_t *) surface->display->backend.data;

	struct wl_display *wl_display = (struct wl_display *)tpl_display->native_handle;

	struct pollfd fds[1];
	int ret;

	/* in this thread we're interesting only in wayland server fd */
	fds[0].fd = wl_display_get_fd(wl_display);
	fds[0].events = POLLIN;

	/* we are in multi-threads environment
	 * NOTE: as I understood another threads doesn't use these approach, it's sad..., if
	 * 	     I've correctly understood multi-thread wayland display fd's reading sequence */
	while (1)
	{
		/* look at wayland-client.c for explanations */
		while (wl_display_prepare_read(wl_display) < 0)
			wl_display_dispatch_queue_pending(wl_display, wayland_display->wl_queue);

		wl_display_flush(wl_display);

		ret = poll(fds, 1, -1);
		if( ret < 0 )
			wl_display_cancel_read(wl_display);
		else
			wl_display_read_events(wl_display);

		wl_display_dispatch_queue_pending(wl_display, wayland_display->wl_queue);
	}
}

//-----------------------------------------------------------------------------------

static tpl_result_t
__tpl_wayland_surface_init(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = NULL;
	struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)
					      surface->native_handle;
	tpl_wayland_display_t *wayland_display =
		(tpl_wayland_display_t *) surface->display->backend.data;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->type == TPL_SURFACE_TYPE_WINDOW);
	TPL_ASSERT(surface->native_handle);

	wayland_surface = (tpl_wayland_surface_t *) calloc(1,
			  sizeof(tpl_wayland_surface_t));
	if (!wayland_surface) {
		TPL_ERR("Failed to allocate memory for new tpl_wayland_surface_t.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	surface->backend.data = (void *)wayland_surface;
	wayland_surface->tbm_queue = NULL;
	wayland_surface->resized = TPL_FALSE;
	wayland_surface->current_buffer = NULL;

	wayland_surface->tbm_queue = tbm_surface_queue_create(CLIENT_QUEUE_SIZE,
				     wl_egl_window->width, wl_egl_window->height, surface->format, 0);

	if (!wayland_surface->tbm_queue) {
		TPL_ERR("TBM surface queue creation failed!");
		free(wayland_surface);
		return TPL_ERROR_INVALID_OPERATION;
	}

	surface->width = wl_egl_window->width;
	surface->height = wl_egl_window->height;

	wl_egl_window->private = surface;
	wl_egl_window->resize_callback = (void *)__cb_client_window_resize_callback;

	/* subscribe for surface's enter/leave events */
	int ret = wl_surface_add_listener(wl_egl_window->surface, &wl_surface_listener, wayland_surface);

	TPL_LOG(3, "wl_surface_add_listener call has been done: wl_surface: %p, ret: %d.",
			wl_egl_window->surface, ret);

	/* all events for this wl_surface will come to @wl_queue queue, so we [main egl thread]
	   will be able to monitor them along buffer release, frame and so on events */
	wl_proxy_set_queue((struct wl_proxy *)wl_egl_window->surface,
			wayland_display->wl_queue);

	/* we need new thread to dispatch wayland queue @wayland_display->wl_queue we've attached
	 * wl_surface object in to be able to monitor surface_leave/surface_enter events.
	 * we can't do it in __tpl_wayland_surface_dequeue_buffer because, in normal case, we
	 * always have buffer to dequeue (tbm_surface_queue isn't empty, so we will not dispatch
	 * wayland queue and it means we cann't react to surface_enter/surface_leave events */
	pthread_create(&wayland_surface->surface_enter_leave_events_thread_id, NULL,
			__tpl_wayland_surface_enter_leave_events_thread, surface);

	TPL_LOG(3, "wl_proxy_set_queue call has been done: wl_surface: %p, wl_event_queue: %p.",
			wl_egl_window->surface, wayland_display->wl_queue);

	/*TPL_LOG(3, "window(%p, %p) %dx%d", surface, surface->native_handle,
		surface->width, surface->height);*/

	return TPL_ERROR_NONE;
}

static void
__tpl_wayland_surface_fini(tpl_surface_t *surface)
{
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_wayland_display_t *wayland_display = NULL;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	wayland_surface = (tpl_wayland_surface_t *) surface->backend.data;
	if (wayland_surface == NULL) return;

	wayland_display = (tpl_wayland_display_t *) surface->display->backend.data;
	if (wayland_display == NULL) return;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	if (surface->type == TPL_SURFACE_TYPE_WINDOW) {
		struct wl_egl_window *wl_egl_window = (struct wl_egl_window *)
						      surface->native_handle;

		TPL_ASSERT(wl_egl_window);
		/* TPL_ASSERT(wl_egl_window->surface); */ /* to be enabled once evas/gl patch is in place */

		wl_egl_window->private = NULL;

		/* Detach all pending buffers */
		if (wl_egl_window->surface &&
		    /* if-statement to be removed once evas/gl patch is in place */
		    wl_egl_window->width == wl_egl_window->attached_width &&
		    wl_egl_window->height == wl_egl_window->attached_height) {

			wl_surface_attach(wl_egl_window->surface, NULL, 0, 0);
			wl_surface_commit(wl_egl_window->surface);
		}

		wl_display_flush(surface->display->native_handle);
		__tpl_wayland_display_roundtrip(surface->display);

		if (wayland_surface->current_buffer)
			tbm_surface_internal_unref(wayland_surface->current_buffer);

		tbm_surface_queue_destroy(wayland_surface->tbm_queue);
		wayland_surface->tbm_queue = NULL;
	}

	free(wayland_surface);
	surface->backend.data = NULL;
}

static tpl_result_t
__tpl_wayland_surface_enqueue_buffer(tpl_surface_t *surface,
				     tbm_surface_h tbm_surface,
				     int num_rects, const int *rects)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);
	TPL_ASSERT(surface->display->native_handle);
	TPL_ASSERT(tbm_surface);

	struct wl_egl_window *wl_egl_window = NULL;
	tpl_wayland_display_t *wayland_display =
		(tpl_wayland_display_t *) surface->display->backend.data;
	tpl_wayland_surface_t *wayland_surface =
		(tpl_wayland_surface_t *) surface->backend.data;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tbm_surface_queue_error_e tsq_err;

	TPL_LOG(3, "window(%p, %p)", surface, surface->native_handle);

	wayland_buffer =
		__tpl_wayland_get_wayland_buffer_from_tbm_surface(tbm_surface);
	TPL_ASSERT(wayland_buffer);

	tbm_bo_handle bo_handle =
		tbm_bo_get_handle(wayland_buffer->bo , TBM_DEVICE_CPU);

	if (bo_handle.ptr)
		TPL_IMAGE_DUMP(bo_handle.ptr, surface->width, surface->height,
			       surface->dump_count++);

	wl_egl_window = (struct wl_egl_window *)surface->native_handle;

	tbm_surface_internal_unref(tbm_surface);

	tsq_err = tbm_surface_queue_enqueue(wayland_surface->tbm_queue, tbm_surface);
	if (tsq_err != TBM_SURFACE_QUEUE_ERROR_NONE) {
		TPL_ERR("Failed to enqeueue tbm_surface. | tsq_err = %d", tsq_err);
		return TPL_ERROR_INVALID_OPERATION;
	}

	/* deprecated */
	tsq_err = tbm_surface_queue_acquire(wayland_surface->tbm_queue, &tbm_surface);
	if (tsq_err != TBM_SURFACE_QUEUE_ERROR_NONE) {
		TPL_ERR("Failed to acquire tbm_surface. | tsq_err = %d", tsq_err);
		return TPL_ERROR_INVALID_OPERATION;
	}

	tbm_surface_internal_ref(tbm_surface);
	wl_surface_attach(wl_egl_window->surface, (void *)wayland_buffer->wl_proxy,
			  wl_egl_window->dx, wl_egl_window->dy);

	wl_egl_window->attached_width = wl_egl_window->width;
	wl_egl_window->attached_height = wl_egl_window->height;

	if (num_rects < 1 || rects == NULL) {
		wl_surface_damage(wl_egl_window->surface,
				  wl_egl_window->dx, wl_egl_window->dy,
				  wl_egl_window->width, wl_egl_window->height);
	} else {
		int i;
		for (i = 0; i < num_rects; i++) {
			wl_surface_damage(wl_egl_window->surface,
					  rects[i * 4 + 0], rects[i * 4 + 1],
					  rects[i * 4 + 2], rects[i * 4 + 3]);
		}
	}

	{
		/* Register a meaningless surface frame callback.
		   Because the buffer_release callback only be triggered if this callback is registered. */
		struct wl_callback *frame_callback = NULL;
		frame_callback = wl_surface_frame(wl_egl_window->surface);
		wl_callback_add_listener(frame_callback, &frame_listener, tbm_surface);
		wl_proxy_set_queue((struct wl_proxy *)frame_callback,
				   wayland_display->wl_queue);
	}
	wl_surface_commit(wl_egl_window->surface);

	wl_display_flush(surface->display->native_handle);

	/* gles is asking to post buffer, so rendering has been done */
	wayland_surface->rendering_is = 0;

	return TPL_ERROR_NONE;
}

static tpl_bool_t
__tpl_wayland_surface_validate(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->backend.data);

	tpl_wayland_surface_t *wayland_surface =
		(tpl_wayland_surface_t *)surface->backend.data;

	if (wayland_surface->resized) return TPL_FALSE;

	return TPL_TRUE;
}

static tbm_surface_h
__tpl_wayland_surface_dequeue_buffer(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->backend.data);
	TPL_ASSERT(surface->display);

	tbm_surface_h tbm_surface = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tpl_wayland_surface_t *wayland_surface =
		(tpl_wayland_surface_t *)surface->backend.data;
	tpl_wayland_display_t *wayland_display =
		(tpl_wayland_display_t *)surface->display->backend.data;
	struct wl_proxy *wl_proxy = NULL;
	tbm_surface_queue_error_e tsq_err = 0;

	if (wayland_surface->resized == TPL_TRUE) wayland_surface->resized = TPL_FALSE;

	/* no reason to unlock surface */
	/* TPL_OBJECT_UNLOCK(surface); */
	while (tbm_surface_queue_can_dequeue(
		       wayland_surface->tbm_queue, 0) == 0) {

		/* queue will be dispatched by another thread */

		/* Application sent all buffers to the server. Wait for server response. */
		/*if (wl_display_dispatch_queue(surface->display->native_handle,
					      wayland_display->wl_queue) == -1) {
			TPL_OBJECT_LOCK(surface);
			return NULL;
		}*/
	}
	/* TPL_OBJECT_LOCK(surface); */

	tsq_err = tbm_surface_queue_dequeue(wayland_surface->tbm_queue, &tbm_surface);
	if (!tbm_surface) {
		TPL_ERR("Failed to get tbm_surface from tbm_surface_queue | tsq_err = %d",
			tsq_err);
		return NULL;
	}

	tbm_surface_internal_ref(tbm_surface);

	if ((wayland_buffer =
		     __tpl_wayland_get_wayland_buffer_from_tbm_surface(tbm_surface)) != NULL) {
		return tbm_surface;
	}

	wayland_buffer = (tpl_wayland_buffer_t *) calloc(1,
			 sizeof(tpl_wayland_buffer_t));
	if (!wayland_buffer) {
		TPL_ERR("Mem alloc for wayland_buffer failed!");
		tbm_surface_internal_unref(tbm_surface);
		return NULL;
	}

	wl_proxy = (struct wl_proxy *)wayland_tbm_client_create_buffer(
			   wayland_display->wl_tbm_client, tbm_surface);
	if (!wl_proxy) {
		TPL_ERR("Failed to create TBM client buffer!");
		tbm_surface_internal_unref(tbm_surface);
		free(wayland_buffer);
		return NULL;
	}

	wl_proxy_set_queue(wl_proxy, wayland_display->wl_queue);
	wl_buffer_add_listener((void *)wl_proxy, &buffer_release_listener,
			       tbm_surface);

	wl_display_flush((struct wl_display *)surface->display->native_handle);

	wayland_buffer->display = surface->display;
	wayland_buffer->wl_proxy = wl_proxy;
	wayland_buffer->bo = tbm_surface_internal_get_bo(tbm_surface, 0);
	wayland_buffer->wayland_surface = wayland_surface;
	wayland_surface->current_buffer = tbm_surface;

	__tpl_wayland_set_wayland_buffer_to_tbm_surface(tbm_surface, wayland_buffer);

	/* gles is requiring buffer, so it will start to render soon */
	wayland_surface->rendering_is = 1;

	return tbm_surface;
}

static void
__tpl_wayland_buffer_free(tpl_wayland_buffer_t *wayland_buffer)
{
	TPL_ASSERT(wayland_buffer);
	TPL_ASSERT(wayland_buffer->display);

	tpl_wayland_display_t *wayland_display =
		(tpl_wayland_display_t *)wayland_buffer->display->backend.data;

	wl_display_flush((struct wl_display *)wayland_buffer->display->native_handle);

	if (wayland_buffer->wl_proxy)
		wayland_tbm_client_destroy_buffer(wayland_display->wl_tbm_client,
						  (void *)wayland_buffer->wl_proxy);

	free(wayland_buffer);
}

tpl_bool_t
__tpl_display_choose_backend_wayland(tpl_handle_t native_dpy)
{
	if (!native_dpy) return TPL_FALSE;

	if (__tpl_wayland_display_is_wl_display(native_dpy))
		return TPL_TRUE;

	return TPL_FALSE;
}

void
__tpl_display_init_backend_wayland(tpl_display_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init = __tpl_wayland_display_init;
	backend->fini = __tpl_wayland_display_fini;
	backend->query_config = __tpl_wayland_display_query_config;
	backend->filter_config = __tpl_wayland_display_filter_config;
	backend->get_window_info = __tpl_wayland_display_get_window_info;
}

void
__tpl_surface_init_backend_wayland(tpl_surface_backend_t *backend)
{
	TPL_ASSERT(backend);

	backend->type = TPL_BACKEND_WAYLAND;
	backend->data = NULL;

	backend->init = __tpl_wayland_surface_init;
	backend->fini = __tpl_wayland_surface_fini;
	backend->validate = __tpl_wayland_surface_validate;
	backend->dequeue_buffer = __tpl_wayland_surface_dequeue_buffer;
	backend->enqueue_buffer = __tpl_wayland_surface_enqueue_buffer;
}

static void
__cb_client_sync_callback(void *data, struct wl_callback *callback,
			  uint32_t serial)
{
	int *done;

	TPL_ASSERT(data);

	done = data;
	*done = 1;

	wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
	__cb_client_sync_callback
};

static void
__cb_client_frame_callback(void *data, struct wl_callback *callback,
			   uint32_t time)
{
	/* We moved the buffer reclaim logic to buffer_release_callback().
	   buffer_release_callback() is more suitable point to delete or reuse buffer instead of frame_callback().
	   But we remain this callback because buffer_release_callback() works only when frame_callback() is activated.*/
	TPL_IGNORE(data);
	TPL_IGNORE(time);

	wl_callback_destroy(callback);
}

static const struct wl_callback_listener frame_listener = {
	__cb_client_frame_callback
};

static void
__cb_client_buffer_release_callback(void *data, struct wl_proxy *proxy)
{
	tpl_wayland_surface_t *wayland_surface = NULL;
	tpl_wayland_buffer_t *wayland_buffer = NULL;
	tbm_surface_h tbm_surface = NULL;

	TPL_ASSERT(data);

	tbm_surface = (tbm_surface_h) data;

	wayland_buffer =
		__tpl_wayland_get_wayland_buffer_from_tbm_surface(tbm_surface);

	if (wayland_buffer) {
		wayland_surface = wayland_buffer->wayland_surface;

		tbm_surface_internal_unref(tbm_surface);

		tbm_surface_queue_release(wayland_surface->tbm_queue, tbm_surface);
	}
}

static const struct wl_buffer_listener buffer_release_listener = {
	(void *)__cb_client_buffer_release_callback,
};

static void
__cb_client_window_resize_callback(struct wl_egl_window *wl_egl_window,
				   void *private)
{
	TPL_ASSERT(private);
	TPL_ASSERT(wl_egl_window);

	int width, height, format;
	tpl_surface_t *surface = (tpl_surface_t *)private;
	tpl_wayland_surface_t *wayland_surface = (tpl_wayland_surface_t *)
			surface->backend.data;

	wayland_surface->resized = TPL_TRUE;

	width = wl_egl_window->width;
	height = wl_egl_window->height;
	format = tbm_surface_queue_get_format(wayland_surface->tbm_queue);

	/* Check whether the surface was resized by wayland_egl */
	if ((wayland_surface->resized == TPL_TRUE)
	    || (width != tbm_surface_queue_get_width(wayland_surface->tbm_queue))
	    || (height != tbm_surface_queue_get_height(wayland_surface->tbm_queue))) {

		if (wayland_surface->current_buffer)
			tbm_surface_internal_unref(wayland_surface->current_buffer);

		tbm_surface_queue_reset(wayland_surface->tbm_queue, width, height, format);
	}
}
