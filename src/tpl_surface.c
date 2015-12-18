#include "tpl_internal.h"

static void
__tpl_surface_fini(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);

	__tpl_region_fini(&surface->damage);
	__tpl_list_fini(&surface->frame_queue, (tpl_free_func_t) __tpl_frame_free);

	if (surface->frame)
		__tpl_frame_free(surface->frame);

	surface->backend.fini(surface);
}

static void
__tpl_surface_free(void *data)
{
	TPL_ASSERT(data);

	__tpl_surface_fini((tpl_surface_t *) data);
	free(data);
}

static tpl_bool_t
__tpl_surface_enqueue_frame(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(surface->frame);

	/* Set swap attributes. */
	surface->frame->interval = surface->post_interval;
	if (TPL_TRUE != __tpl_region_copy(&surface->frame->damage, &surface->damage))
		return TPL_FALSE;

	/* Enqueue the frame object.*/
	if (TPL_TRUE != __tpl_list_push_back(&surface->frame_queue, surface->frame))
		return TPL_FALSE;

	surface->frame->state = TPL_FRAME_STATE_QUEUED;

	/* Reset surface frame to NULL. */
	if (surface->backend.end_frame)
		surface->backend.end_frame(surface);

	surface->frame = NULL;

	return TPL_TRUE;
}

tpl_frame_t *
__tpl_surface_get_latest_frame(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);

	if (__tpl_list_is_empty(&surface->frame_queue))
		return NULL;

	return (tpl_frame_t *) __tpl_list_get_back(&surface->frame_queue);
}

void
__tpl_surface_wait_all_frames(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);

	while (!__tpl_list_is_empty(&surface->frame_queue))
	{
		TPL_OBJECT_UNLOCK(surface);
		__tpl_util_sys_yield();
		TPL_OBJECT_LOCK(surface);
	}
}

tpl_surface_t *
tpl_surface_create(tpl_display_t *display, tpl_handle_t handle, tpl_surface_type_t type, tpl_format_t format)
{
	tpl_surface_t *surface;

	if (NULL == display)
	{
		TPL_ERR("Display is NULL!");
		return NULL;
	}

	if (NULL == handle)
	{
		TPL_ERR("Handle is NULL!");
		return NULL;
	}

	surface = (tpl_surface_t *) calloc(1, sizeof(tpl_surface_t));
	if (NULL == surface)
	{
		TPL_ERR("Failed to allocate memory for surface!");
		return NULL;
	}

	TPL_LOG(5, "surface->damage:%p {%d, %p, %p, %d}", &surface->damage, surface->damage.num_rects,
		surface->damage.rects, &surface->damage.rects_static[0], surface->damage.num_rects_allocated);

	if (TPL_TRUE != __tpl_object_init(&surface->base, TPL_OBJECT_SURFACE, __tpl_surface_free))
	{
		TPL_ERR("Failed to initialize surface's base class!");
		free(surface);
		return NULL;
	}

	surface->display = display;
	surface->native_handle = handle;
	surface->type = type;
	surface->format = format;

	surface->frame = NULL;
	surface->post_interval = 1;

        surface->dump_count = 0;
	__tpl_region_init(&surface->damage);
	__tpl_list_init(&surface->frame_queue);

	/* Intialize backend. */
	__tpl_surface_init_backend(surface, display->backend.type);

	if (NULL == surface->backend.init || TPL_TRUE != surface->backend.init(surface))
	{
		TPL_ERR("Failed to initialize surface's backend!");
		tpl_object_unreference(&surface->base);
		return NULL;
	}

	return surface;
}

tpl_display_t *
tpl_surface_get_display(tpl_surface_t *surface)
{
	if (NULL == surface)
	{
		TPL_ERR("Surface is NULL!");
		return NULL;
	}

	return surface->display;
}

tpl_handle_t
tpl_surface_get_native_handle(tpl_surface_t *surface)
{
	if (NULL == surface)
	{
		TPL_ERR("Surface is NULL!");
		return NULL;
	}

	return surface->native_handle;
}

tpl_surface_type_t
tpl_surface_get_type(tpl_surface_t *surface)
{
	if (NULL == surface)
	{
		TPL_ERR("Surface is NULL!");
		return TPL_SURFACE_ERROR;
	}

	return surface->type;
}

tpl_bool_t
tpl_surface_get_size(tpl_surface_t *surface, int *width, int *height)
{
	if (NULL == surface)
	{
		TPL_ERR("Surface is NULL!");
		return TPL_FALSE;
	}

	if (width)
		*width = surface->width;

	if (height)
		*height = surface->height;

	return TPL_TRUE;
}

tpl_bool_t
tpl_surface_begin_frame(tpl_surface_t *surface)
{
	if (NULL == surface)
	{
		TPL_ERR("Surface is NULL!");
		return TPL_FALSE;
	}

	if (TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Surface is not of type window!");
		return TPL_FALSE;
	}

	TRACE_BEGIN("TPL:BEGINFRAME");
	TPL_OBJECT_LOCK(surface);

	/* Queue previous frame if it has not been queued. */
	if (surface->frame)
	{
		if (TPL_TRUE != __tpl_surface_enqueue_frame(surface))
		{
			TPL_OBJECT_UNLOCK(surface);
			TRACE_END();
			TPL_ERR("Failed to enqueue frame!");
			return TPL_FALSE;
		}
	}

	/* Allocate a new frame. */
	surface->frame = __tpl_frame_alloc();
	if (NULL == surface->frame)
	{
		TPL_OBJECT_UNLOCK(surface);
		TRACE_END();
		TPL_ERR("Failed to allocate frame!");
		return TPL_FALSE;
	}

	surface->frame->state = TPL_FRAME_STATE_READY;

	TPL_LOG(5, "surface->frame:%p, surface->damage:%p, surface->frame->damage:%p",
		surface->frame, &surface->damage, (surface->frame)?(&surface->frame->damage):NULL);
	/* There might be some frames which is enqueued but not posted. Some backend requires native
	 * surface to be posted to be able to get the next output buffer (i.e. x11_dri2). Runtime
	 * just request buffer for the frame and it is totally upto backend if it calls post
	 * internally or not.
	 *
	 * In case of backend calling internal post, backend must mark the frame as posted.
	 * tpl_surface_post() will skip calling backend post if the frame is marked as posted and
	 * it will be just removed from the queue. */

	/* Let backend handle the new frame event. */
	if (surface->backend.begin_frame)
		surface->backend.begin_frame(surface);

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return TPL_TRUE;
}

tpl_bool_t
tpl_surface_end_frame(tpl_surface_t *surface)
{
	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	TPL_LOG(5, "surface->frame:%p, surface->damage:%p, surface->frame->damage:%p",
		surface->frame, &surface->damage, (surface->frame)?(&surface->frame->damage):NULL);

	TRACE_BEGIN("TPL:ENDFRAME");
	TPL_OBJECT_LOCK(surface);

	if (surface->frame)
	{
		if (TPL_TRUE != __tpl_surface_enqueue_frame(surface))
		{
			TPL_OBJECT_UNLOCK(surface);
			TRACE_END();
			TPL_ERR("Failed to enqueue frame!");
			return TPL_FALSE;
		}
	}

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return TPL_TRUE;
}

tpl_bool_t
tpl_surface_validate_frame(tpl_surface_t *surface)
{
	tpl_bool_t was_valid = TPL_TRUE;

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	if (NULL == surface->frame)
	{
		TPL_ERR("Frame not registered in surface!");
		return TPL_FALSE;
	}

	if (NULL == surface->backend.validate_frame)
	{
		TPL_ERR("Backend for surface has not been initialized!");
		return TPL_FALSE;
	}

	TRACE_BEGIN("TPL:VALIDATEFRAME");
	TPL_OBJECT_LOCK(surface);

	if (!surface->backend.validate_frame(surface))
		was_valid = TPL_FALSE;

	TPL_LOG(5, "surface->frame:%p, surface->damage:%p, %s",
			surface->frame, &surface->damage,
			was_valid?"VALID_FRAME":"INVALID_FRAME");

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return was_valid;
}

tpl_bool_t
tpl_surface_set_post_interval(tpl_surface_t *surface, int interval)
{
	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(surface);
	surface->post_interval = interval;
	TPL_OBJECT_UNLOCK(surface);

	return TPL_TRUE;
}

int
tpl_surface_get_post_interval(tpl_surface_t *surface)
{
	int interval;

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return -1;
	}

	TPL_OBJECT_LOCK(surface);
	interval = surface->post_interval;
	TPL_OBJECT_UNLOCK(surface);

	return interval;
}

tpl_bool_t
tpl_surface_set_damage(tpl_surface_t *surface, int num_rects, const int *rects)
{
	tpl_bool_t ret;

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(surface);
	ret = __tpl_region_set_rects(&surface->damage, num_rects, rects);
	TPL_OBJECT_UNLOCK(surface);

	return ret;
}

tpl_bool_t
tpl_surface_get_damage(tpl_surface_t *surface, int *num_rects, const int **rects)
{
	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		*num_rects = 0;
		*rects = NULL;
		return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(surface);
	*num_rects = surface->damage.num_rects;
	*rects = surface->damage.rects;
	TPL_OBJECT_UNLOCK(surface);

	return TPL_TRUE;
}

tpl_buffer_t *
tpl_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	tpl_buffer_t *buffer = NULL;

	if (NULL == surface)
	{
		TPL_ERR("Invalid surface!");
		return NULL;
	}

	if (NULL == surface->backend.get_buffer)
	{
		TPL_ERR("TPL surface has not been initialized correctly!");
		return NULL;
	}

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

tpl_bool_t
tpl_surface_destroy_cached_buffers(tpl_surface_t *surface)
{
	tpl_bool_t retval = TPL_FALSE;

	if (NULL == surface)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	if (NULL == surface->backend.destroy_cached_buffers)
	{
		TPL_ERR("TPL surface has not been initialized correctly!");
		return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(surface);
	retval = surface->backend.destroy_cached_buffers(surface);
	TPL_OBJECT_UNLOCK(surface);

	return retval;
}

tpl_bool_t
tpl_surface_update_cached_buffers(tpl_surface_t *surface)
{
	tpl_bool_t retval = TPL_FALSE;

	if (NULL == surface)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	if (NULL == surface->backend.destroy_cached_buffers)
	{
		TPL_ERR("TPL surface has not been initialized correctly!");
		return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(surface);
	retval = surface->backend.update_cached_buffers(surface);
	TPL_OBJECT_UNLOCK(surface);

	return retval;
}

tpl_bool_t
tpl_surface_post(tpl_surface_t *surface)
{
	tpl_frame_t *frame;

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	TPL_LOG(5, "surface->frame:%p, surface->damage:%p, surface->frame->damage:%p",
		surface->frame, &surface->damage, (surface->frame)?(&surface->frame->damage):NULL);

	TRACE_BEGIN("TPL:POST");
	TPL_OBJECT_LOCK(surface);

	if (__tpl_list_is_empty(&surface->frame_queue))
	{
		/* Queue is empty and swap is called.
		 * This means that current frame is not enqueued yet
		 * and there's no pending frames.
		 * So, this post is for the current frame. */
		if (NULL == surface->frame)
		{
			TPL_OBJECT_UNLOCK(surface);
			TRACE_END();
			TPL_ERR("Frame not registered in surface!");
			return TPL_FALSE;
		}

		if (TPL_TRUE != __tpl_surface_enqueue_frame(surface))
		{
			TPL_OBJECT_UNLOCK(surface);
			TRACE_END();
			TPL_ERR("Failed to enqueue frame!");
			return TPL_FALSE;
		}
	}

	/* Dequeue a frame from the queue. */
	frame = (tpl_frame_t *) __tpl_list_pop_front(&surface->frame_queue, NULL);

	if (NULL == frame)
	{
		TPL_OBJECT_UNLOCK(surface);
		TRACE_END();
		TPL_ERR("Frame to post received from the queue is invalid!");
		return TPL_FALSE;
	}

	if (frame->buffer == NULL)
	{
		__tpl_frame_free(frame);
		TPL_OBJECT_UNLOCK(surface);
		TRACE_END();
		TPL_ERR("Buffer not initialized for frame!");
		return TPL_FALSE;
	}

	/* Call backend post if it has not been called for the frame. */
	if (TPL_FRAME_STATE_POSTED != frame->state)
		surface->backend.post(surface, frame);

	__tpl_frame_free(frame);
	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return TPL_TRUE;
}
