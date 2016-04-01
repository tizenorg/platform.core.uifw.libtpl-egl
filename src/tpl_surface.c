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
tpl_surface_create(tpl_display_t *display, tpl_handle_t handle,
		   tpl_surface_type_t type, tbm_format format)
{
	tpl_surface_t *surface;

	if (!display) {
		TPL_ERR("Display is NULL!");
		return NULL;
	}

	if (!handle) {
		TPL_ERR("Handle is NULL!");
		return NULL;
	}

	surface = (tpl_surface_t *) calloc(1, sizeof(tpl_surface_t));
	if (!surface) {
		TPL_ERR("Failed to allocate memory for surface!");
		return NULL;
	}

	if (__tpl_object_init(&surface->base, TPL_OBJECT_SURFACE,
			      __tpl_surface_free) != TPL_ERROR_NONE) {
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

	if ((!surface->backend.init)
	    || (surface->backend.init(surface) != TPL_ERROR_NONE)) {
		TPL_ERR("Failed to initialize surface's backend!");
		tpl_object_unreference(&surface->base);
		return NULL;
	}

	return surface;
}

tpl_display_t *
tpl_surface_get_display(tpl_surface_t *surface)
{
	if (!surface) {
		TPL_ERR("Surface is NULL!");
		return NULL;
	}

	return surface->display;
}

tpl_handle_t
tpl_surface_get_native_handle(tpl_surface_t *surface)
{
	if (!surface) {
		TPL_ERR("Surface is NULL!");
		return NULL;
	}

	return surface->native_handle;
}

tpl_surface_type_t
tpl_surface_get_type(tpl_surface_t *surface)
{
	if (!surface) {
		TPL_ERR("Surface is NULL!");
		return TPL_SURFACE_ERROR;
	}

	return surface->type;
}

tpl_result_t
tpl_surface_get_size(tpl_surface_t *surface, int *width, int *height)
{
	if (!surface) {
		TPL_ERR("Surface is NULL!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (width) *width = surface->width;

	if (height) *height = surface->height;

	return TPL_ERROR_NONE;
}


tpl_bool_t
tpl_surface_validate(tpl_surface_t *surface)
{
	tpl_bool_t was_valid = TPL_TRUE;

	if (!surface || (surface->type != TPL_SURFACE_TYPE_WINDOW)) {
		TPL_ERR("Invalid surface!");
		return TPL_FALSE;
	}

	if (!surface->backend.validate) {
		TPL_ERR("Backend for surface has not been initialized!");
		return TPL_FALSE;
	}

	TRACE_BEGIN("TPL:VALIDATEFRAME");
	TPL_OBJECT_LOCK(surface);

	if (!surface->backend.validate(surface)) was_valid = TPL_FALSE;

	TPL_OBJECT_UNLOCK(surface);
	TRACE_END();

	return was_valid;
}

tpl_result_t
tpl_surface_set_post_interval(tpl_surface_t *surface, int interval)
{
	if (!surface || (surface->type != TPL_SURFACE_TYPE_WINDOW)) {
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

	if (!surface || (surface->type != TPL_SURFACE_TYPE_WINDOW)) {
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

	if (!surface->backend.dequeue_buffer) {
		TPL_ERR("TPL surface has not been initialized correctly!");
		return NULL;
	}

	TRACE_BEGIN("TPL:GETBUFFER");
	TPL_OBJECT_LOCK(surface);

	tbm_surface = surface->backend.dequeue_buffer(surface);

	if (tbm_surface) {
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

	if (!surface || (surface->type != TPL_SURFACE_TYPE_WINDOW)) {
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	TRACE_BEGIN("TPL:POST");
	TPL_OBJECT_LOCK(surface);

	if (!tbm_surface) {
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
tpl_surface_enqueue_buffer_with_damage(tpl_surface_t *surface,
				       tbm_surface_h tbm_surface,
				       int num_rects, const int *rects)
{
	tpl_result_t ret = TPL_ERROR_INVALID_OPERATION;

	if (!surface || (surface->type != TPL_SURFACE_TYPE_WINDOW)) {
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	TRACE_BEGIN("TPL:POST");
	TPL_OBJECT_LOCK(surface);

	if (!tbm_surface) {
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

tpl_result_t
tpl_surface_query_supported_buffer_count(tpl_surface_t *surface, int *min,
		int *max)
{
	if (!surface || (surface->type != TPL_SURFACE_TYPE_WINDOW)) {
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (min) *min = surface->capabilities.min_buffer;
	if (max) *max = surface->capabilities.max_buffer;

	return TPL_ERROR_NONE;

}

tpl_result_t
tpl_surface_get_swapchain_buffers(tpl_surface_t *surface,
				  tbm_surface_h **buffers, int *buffer_count)
{
	tpl_result_t ret = TPL_ERROR_INVALID_OPERATION;

	if (!surface || (surface->type != TPL_SURFACE_TYPE_WINDOW)) {
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (!buffer_count) {
		TPL_ERR("Invalid buffer_count!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (!surface->backend.get_swapchain_buffers) {
		TPL_ERR("Backend does not support!");
		return TPL_ERROR_INVALID_OPERATION;
	}

	TPL_OBJECT_LOCK(surface);

	ret = surface->backend.get_swapchain_buffers(surface, buffers, buffer_count);

	TPL_OBJECT_UNLOCK(surface);

	return ret;
}

tpl_result_t
tpl_surface_create_swapchain(tpl_surface_t *surface, tbm_format format,
			     int width, int height, int buffer_count)
{
	tpl_result_t ret = TPL_ERROR_INVALID_OPERATION;

	if (!surface) {
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if ((width <= 0) || (height <= 0) ) {
		TPL_ERR("Invalid width or  height!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if ((buffer_count < surface->capabilities.min_buffer)
	    || (buffer_count > surface->capabilities.max_buffer)) {
		TPL_ERR("Invalid buffer_count!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (!surface->backend.create_swapchain) {
		TPL_ERR("Backend does not support!");
		return TPL_ERROR_INVALID_OPERATION;
	}

	TPL_OBJECT_LOCK(surface);

	ret = surface->backend.create_swapchain(surface, format, width, height,
						buffer_count);

	TPL_OBJECT_UNLOCK(surface);

	return ret;
}

tpl_result_t
tpl_surface_destroy_swapchain(tpl_surface_t *surface)
{
	tpl_result_t ret = TPL_ERROR_INVALID_OPERATION;

	if (!surface) {
		TPL_ERR("Invalid surface!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	if (!surface->backend.destroy_swapchain) {
		TPL_ERR("Backend does not support!");
		return TPL_ERROR_INVALID_OPERATION;
	}

	TPL_OBJECT_LOCK(surface);

	ret = surface->backend.destroy_swapchain(surface);

	TPL_OBJECT_UNLOCK(surface);

	return ret;
}
