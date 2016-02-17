#include "tpl_internal.h"

static void
__tpl_surface_fini(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);

	surface->backend.fini(surface);
}

static void
__tpl_surface_free(void *data)
{
	TPL_ASSERT(data);
	__tpl_surface_fini((tpl_surface_t *) data);
	free(data);
}

tpl_surface_t *
tpl_surface_create(tpl_display_t *display, tpl_handle_t handle, tpl_surface_type_t type, tbm_format format)
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

	if (TPL_ERROR_NONE != __tpl_object_init(&surface->base, TPL_OBJECT_SURFACE, __tpl_surface_free))
	{
		TPL_ERR("Failed to initialize surface's base class!");
		free(surface);
		return NULL;
	}

	surface->display = display;
	surface->native_handle = handle;
	surface->type = type;
	surface->format = format;

	surface->post_interval = 1;

        surface->dump_count = 0;

	/* Intialize backend. */
	__tpl_surface_init_backend(surface, display->backend.type);

	if (NULL == surface->backend.init || TPL_ERROR_NONE != surface->backend.init(surface))
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

tpl_result_t
tpl_surface_get_size(tpl_surface_t *surface, int *width, int *height)
{
	if (NULL == surface)
	{
		TPL_ERR("Surface is NULL!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (width)
		*width = surface->width;

	if (height)
		*height = surface->height;

	return TPL_ERROR_NONE;
}


tpl_bool_t
tpl_surface_validate(tpl_surface_t *surface)
{
	tpl_bool_t was_valid = TPL_TRUE;

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	if (NULL == surface->backend.validate)
	{
		TPL_ERR("Backend for surface has not been initialized!");
		return TPL_FALSE;
	}

	TRACE_BEGIN("TPL:VALIDATEFRAME");
	TPL_OBJECT_LOCK(surface);

	if (!surface->backend.validate(surface))
		was_valid = TPL_FALSE;

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return was_valid;
}

tpl_result_t
tpl_surface_set_post_interval(tpl_surface_t *surface, int interval)
{
	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	TPL_OBJECT_LOCK(surface);
	surface->post_interval = interval;
	TPL_OBJECT_UNLOCK(surface);

	return TPL_ERROR_NONE;
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

tbm_surface_h
tpl_surface_dequeue_buffer(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);

	tbm_surface_h tbm_surface = NULL;

	if (surface->backend.dequeue_buffer == NULL)
	{
		TPL_ERR("TPL surface has not been initialized correctly!");
		return NULL;
	}

	TRACE_BEGIN("TPL:GETBUFFER");
	TPL_OBJECT_LOCK(surface);

	tbm_surface = surface->backend.dequeue_buffer(surface);

	if(tbm_surface != NULL)
	{
		/* Update size of the surface. */
		surface->width = tbm_surface_get_width(tbm_surface);
		surface->height = tbm_surface_get_height(tbm_surface);
	}

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return tbm_surface;
}

tpl_result_t
tpl_surface_enqueue_buffer(tpl_surface_t *surface, tbm_surface_h tbm_surface)
{
	tpl_result_t ret = TPL_ERROR_INVALID_OPERATION;

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	TRACE_BEGIN("TPL:POST");
	TPL_OBJECT_LOCK(surface);

	if (NULL == tbm_surface)
	{
		TPL_OBJECT_UNLOCK(surface);
		TRACE_END();
		TPL_ERR("tbm surface is invalid.");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	/* Call backend post */
	ret = surface->backend.enqueue_buffer(surface, tbm_surface, 0, NULL);

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return ret;
}

tpl_result_t
tpl_surface_enqueue_buffer_with_damage(tpl_surface_t *surface, tbm_surface_h tbm_surface,
				       int num_rects, const int *rects)
{
	tpl_result_t ret = TPL_ERROR_INVALID_OPERATION;

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	TRACE_BEGIN("TPL:POST");
	TPL_OBJECT_LOCK(surface);

	if (NULL == tbm_surface)
	{
		TPL_OBJECT_UNLOCK(surface);
		TRACE_END();
		TPL_ERR("tbm surface is invalid.");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	/* Call backend post */
	ret = surface->backend.enqueue_buffer(surface, tbm_surface, num_rects, rects);

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return ret;
}
