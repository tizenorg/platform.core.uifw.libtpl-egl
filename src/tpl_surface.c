#include "tpl_internal.h"

static void
__tpl_surface_fini(tpl_surface_t *surface)
{
	TPL_ASSERT(surface);

	__tpl_region_fini(&surface->damage);

	surface->backend.fini(surface);
}

static void
__tpl_surface_free(void *data)
{
	TPL_ASSERT(data);
        TPL_LOG(9, "tpl_surface_t(%p)", data);
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

	surface->post_interval = 1;

        surface->dump_count = 0;
	__tpl_region_init(&surface->damage);

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

tbm_surface_h
tpl_surface_get_buffer(tpl_surface_t *surface, tpl_bool_t *reset_buffers)
{
	TPL_ASSERT(surface);

	tbm_surface_h tbm_surface = NULL;

	if (surface->backend.get_buffer == NULL)
	{
		TPL_ERR("TPL surface has not been initialized correctly!");
		return NULL;
	}

	TRACE_BEGIN("TPL:GETBUFFER");
	TPL_OBJECT_LOCK(surface);

	tbm_surface = surface->backend.get_buffer(surface, reset_buffers);

	if(tbm_surface != NULL)
	{
		/* Update size of the surface. */
		surface->width = tbm_surface_internal_get_width(tbm_surface);
		surface->height = tbm_surface_internal_get_height(tbm_surface);
	}

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return (void*)tbm_surface;
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
tpl_surface_post(tpl_surface_t *surface, tbm_surface_h tbm_surface)
{

	if (NULL == surface || TPL_SURFACE_TYPE_WINDOW != surface->type)
	{
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	TRACE_BEGIN("TPL:POST");
	TPL_OBJECT_LOCK(surface);

	if (NULL == tbm_surface)
	{
		TPL_OBJECT_UNLOCK(surface);
		TRACE_END();
		TPL_ERR("tbm surface is invalid.");
		return TPL_FALSE;
	}

	/* Call backend post if it has not been called for the frame. */
	surface->backend.post(surface, tbm_surface);

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return TPL_TRUE;
}
