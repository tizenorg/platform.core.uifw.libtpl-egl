#include "tpl_internal.h"

static void
__tpl_surface_fini(tpl_surface_t *surface)
{
	tpl_region_fini(&surface->damage);
	tpl_list_fini(&surface->frame_queue, (tpl_free_func_t)__tpl_frame_free);

	if (surface->frame)
		__tpl_frame_free(surface->frame);

	surface->backend.fini(surface);
}

static void
__tpl_surface_free(void *data)
{
	__tpl_surface_fini((tpl_surface_t *)data);
	free(data);
}

static void
__tpl_surface_enqueue_frame(tpl_surface_t *surface)
{
	/* Set swap attributes. */
	surface->frame->interval = surface->post_interval;
	tpl_region_copy(&surface->frame->damage, &surface->damage);

	/* Enqueue the frame object. */
	tpl_list_push_back(&surface->frame_queue, surface->frame);
	surface->frame->state = TPL_FRAME_STATE_QUEUED;

	/* Reset surface frame to NULL. */
	surface->backend.end_frame(surface);
	surface->frame = NULL;
}

tpl_frame_t *
__tpl_surface_get_latest_frame(tpl_surface_t *surface)
{
	if (tpl_list_is_empty(&surface->frame_queue))
		return NULL;

	return (tpl_frame_t *)tpl_list_get_back(&surface->frame_queue);
}

void
__tpl_surface_wait_all_frames(tpl_surface_t *surface)
{
	while (!tpl_list_is_empty(&surface->frame_queue))
	{
		TPL_OBJECT_UNLOCK(surface);
		tpl_util_sys_yield();
		TPL_OBJECT_LOCK(surface);
	}
}

tpl_surface_t *
tpl_surface_create(tpl_display_t *display, tpl_handle_t handle, tpl_surface_type_t type, tpl_format_t format)
{
	tpl_surface_t *surface;

	TPL_ASSERT(display != NULL);

	surface = (tpl_surface_t *)calloc(1, sizeof(tpl_surface_t));
	TPL_ASSERT(surface != NULL);

	TPL_LOG(3, "surface->damage:%p {%d, %p, %p, %d}\n", &surface->damage, surface->damage.num_rects,
		surface->damage.rects, &surface->damage.rects_static[0], surface->damage.num_rects_allocated);

	__tpl_object_init(&surface->base, TPL_OBJECT_SURFACE, __tpl_surface_free);

	surface->display = display;
	surface->native_handle = handle;
	surface->type = type;
	surface->format = format;

	surface->frame = NULL;
	surface->post_interval = 1;

	tpl_region_init(&surface->damage);
	tpl_list_init(&surface->frame_queue);

	/* Intialize backend. */
	__tpl_surface_init_backend(surface, display->backend.type);

	if (!surface->backend.init(surface))
	{
		tpl_object_unreference(&surface->base);
		return NULL;
	}

	return surface;
}

tpl_display_t *
tpl_surface_get_display(tpl_surface_t *surface)
{
	return surface->display;
}

tpl_handle_t
tpl_surface_get_native_handle(tpl_surface_t *surface)
{
	return surface->native_handle;
}

tpl_surface_type_t
tpl_surface_get_type(tpl_surface_t *surface)
{
	return surface->type;
}

void
tpl_surface_get_size(tpl_surface_t *surface, int *width, int *height)
{
	if (width)
		*width = surface->width;

	if (height)
		*height = surface->height;
}

void
tpl_surface_begin_frame(tpl_surface_t *surface)
{
	TRACE_BEGIN("TPL:BEGINFRAME");

	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
	{
		TRACE_END();
		return;
	}

	TPL_OBJECT_LOCK(surface);

	/* Queue previous frame if it has not been queued. */
	if (surface->frame)
		__tpl_surface_enqueue_frame(surface);

	/* Allocate a new frame. */
	surface->frame = __tpl_frame_alloc();
	TPL_ASSERT(surface->frame != NULL);

	surface->frame->state = TPL_FRAME_STATE_READY;

	/* There might be some frames which is enqueued but not posted. Some backend requires native
	 * surface to be posted to be able to get the next output buffer (i.e. x11_dri2). Runtime
	 * just request buffer for the frame and it is totally upto backend if it calls post
	 * internally or not.
	 *
	 * In case of backend calling internal post, backend must mark the frame as posted.
	 * tpl_surface_post() will skip calling backend post if the frame is marked as posted and
	 * it will be just removed from the queue. */

	/* Let backend handle the new frame event. */
	surface->backend.begin_frame(surface);

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();
}

void
tpl_surface_end_frame(tpl_surface_t *surface)
{
	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return;

	TRACE_BEGIN("TPL:ENDFRAME");
	TPL_OBJECT_LOCK(surface);

	TPL_LOG(3, "surface->frame:%p, surface->damage:%p, surface->frame->damage:%p",
		surface->frame, &surface->damage, (surface->frame)?(&surface->frame->damage):NULL);

	if (surface->frame)
		__tpl_surface_enqueue_frame(surface);

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();
}

tpl_bool_t
tpl_surface_validate_frame(tpl_surface_t *surface)
{
	tpl_bool_t was_valid = TPL_TRUE;

	TRACE_BEGIN("TPL:VALIDATEFRAME");

	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
        {
		TRACE_END();
		return was_valid;
        }

	TPL_OBJECT_LOCK(surface);
	TPL_ASSERT(surface->frame != NULL);

	if (!surface->backend.validate_frame(surface))
		was_valid = TPL_FALSE;

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();
	return was_valid;
}

void
tpl_surface_set_post_interval(tpl_surface_t *surface, int interval)
{
	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return;

	TPL_OBJECT_LOCK(surface);
	surface->post_interval = interval;
	TPL_OBJECT_UNLOCK(surface);
}

int
tpl_surface_get_post_interval(tpl_surface_t *surface)
{
	int interval;

	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return 0;

	TPL_OBJECT_LOCK(surface);
	interval = surface->post_interval;
	TPL_OBJECT_UNLOCK(surface);

	return interval;
}

void
tpl_surface_set_damage(tpl_surface_t *surface, int num_rects, const int *rects)
{
	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
		return;

	TPL_OBJECT_LOCK(surface);
	tpl_region_set_rects(&surface->damage, num_rects, rects);
	TPL_OBJECT_UNLOCK(surface);
}

void
tpl_surface_get_damage(tpl_surface_t *surface, int *num_rects, const int **rects)
{
	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
	{
		*num_rects = 0;
		*rects = NULL;
		return;
	}

	TPL_OBJECT_LOCK(surface);
	*num_rects = surface->damage.num_rects;
	*rects = surface->damage.rects;
	TPL_OBJECT_UNLOCK(surface);
}

tpl_buffer_t *
tpl_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;

	TRACE_BEGIN("TPL:GETBUFFER");
	TPL_OBJECT_LOCK(surface);

	buffer = surface->backend.get_buffer(surface, reset_buffers);

	if(buffer != NULL)
	{
		/* Update size of the surface. */
		surface->width = buffer->width;
		surface->height = buffer->height;

		if (surface->frame)
			__tpl_frame_set_buffer(surface->frame, buffer);
	}

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return buffer;
}

void
tpl_surface_post(tpl_surface_t *surface)
{
	tpl_frame_t *frame;

        TRACE_BEGIN("TPL:POST");
	if (surface->type != TPL_SURFACE_TYPE_WINDOW)
	{
		TRACE_END();
		return;
	}

	TPL_OBJECT_LOCK(surface);

	TPL_LOG(3, "surface->frame:%p, surface->damage:%p, surface->frame->damage:%p",
		surface->frame, &surface->damage, (surface->frame)?(&surface->frame->damage):NULL);

	if (tpl_list_is_empty(&surface->frame_queue))
	{
		/* Queue is empty and swap is called.
		 * This means that current frame is not enqueued yet
		 * and there's no pending frames.
		 * So, this post is for the current frame. */
		TPL_ASSERT(surface->frame != NULL);

		__tpl_surface_enqueue_frame(surface);
	}

	/* Dequeue a frame from the queue. */
	frame = (tpl_frame_t *)tpl_list_pop_front(&surface->frame_queue, NULL);

	/* Call backend post if it has not been called for the frame. */
	if (frame->state != TPL_FRAME_STATE_POSTED)
		surface->backend.post(surface, frame);

	__tpl_frame_free(frame);
	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();
}
