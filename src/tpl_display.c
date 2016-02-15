#include "tpl_internal.h"

static void
__tpl_display_fini(tpl_display_t *display)
{
	TPL_ASSERT(display);

	if (display->backend.fini)
		display->backend.fini(display);

	__tpl_runtime_remove_display(display);
}

static void
__tpl_display_free(void *display)
{
	TPL_ASSERT(display);

	__tpl_display_fini((tpl_display_t *)display);
	free(display);
}

tpl_display_t *
tpl_display_get(tpl_backend_type_t type, tpl_handle_t native_dpy)
{
	tpl_display_t *display;
	tpl_bool_t ret;

	/* Search for an already connected display for the given native display. */
	display = __tpl_runtime_find_display(type, native_dpy);

	/* exact match found, can return immediately */
	if (NULL != display)
		return display;

	/* if backend is unknown, try to find the best match from the list of supported types */
	if (TPL_BACKEND_UNKNOWN == type)
		type = __tpl_display_choose_backend(native_dpy);

	/* if still not found, then there's no compatible display */
	if (TPL_BACKEND_UNKNOWN == type || TPL_BACKEND_COUNT == type || TPL_BACKEND_MAX <= type)
	{
		TPL_ERR("Invalid backend type!");
		return NULL;
	}

	display = (tpl_display_t *)calloc(1, sizeof(tpl_display_t));
	if (NULL == display)
	{
		TPL_ERR("Failed to allocate memory for display!");
		return NULL;
	}

	/* Initialize object base class. */
	ret = __tpl_object_init(&display->base, TPL_OBJECT_DISPLAY, __tpl_display_free);
	if (TPL_TRUE != ret)
	{
		TPL_ERR("Failed to initialize display's base class!");
		free(display);
		return NULL;
	}

	/* Initialize display object. */
	display->native_handle = native_dpy;

	/* Initialize backend. */
	__tpl_display_init_backend(display, type);

	if (!display->backend.init(display))
	{
		TPL_ERR("Failed to initialize display's backend!");
		tpl_object_unreference((tpl_object_t *) display);
		return NULL;
	}

	/* Add it to the runtime. */
	ret = __tpl_runtime_add_display(display);
	if (TPL_TRUE != ret)
	{
		TPL_ERR("Failed to add display to runtime list!");
		tpl_object_unreference((tpl_object_t *) display);
		return NULL;
	}

	return display;
}

tpl_bool_t
tpl_display_bind_client_display_handle(tpl_display_t *display, tpl_handle_t native_dpy)
{
	if(NULL == display || TPL_TRUE != __tpl_object_is_valid(&display->base) || NULL == display->backend.bind_client_display_handle)
	{
		TPL_ERR("display is invalid!");
		return TPL_FALSE;
	}

	if (NULL == native_dpy)
	{
		TPL_ERR("native_dpy is NULL!");
		return TPL_FALSE;
	}

	return display->backend.bind_client_display_handle(display, native_dpy);
}

tpl_bool_t
tpl_display_unbind_client_display_handle(tpl_display_t *display, tpl_handle_t native_dpy)
{
	if(NULL == display || TPL_TRUE != __tpl_object_is_valid(&display->base) || NULL == display->backend.unbind_client_display_handle)
	{
		TPL_ERR("display is invalid!");
		return TPL_FALSE;
	}

	if (NULL == native_dpy)
	{
		TPL_ERR("native_dpy is NULL!");
		return TPL_FALSE;
	}

	return display->backend.unbind_client_display_handle(display, native_dpy);
}

tpl_handle_t
tpl_display_get_native_handle(tpl_display_t *display)
{
	if(NULL == display || TPL_TRUE != __tpl_object_is_valid(&display->base))
	{
		TPL_ERR("display is invalid!");
		return NULL;
	}

	return display->native_handle;
}

tpl_bool_t
tpl_display_query_config(tpl_display_t *display,
			 tpl_surface_type_t surface_type,
			 int red_size,
			 int green_size,
			 int blue_size,
			 int alpha_size,
			 int depth_size,
			 int *native_visual_id,
			 tpl_bool_t *is_slow)
{
	if(NULL == display || TPL_TRUE != __tpl_object_is_valid(&display->base) || NULL == display->backend.query_config)
	{
		TPL_ERR("display is invalid!");
		return TPL_FALSE;
	}

	return display->backend.query_config(display, surface_type, red_size, green_size, blue_size, alpha_size, depth_size, native_visual_id, is_slow);
}

tpl_bool_t
tpl_display_filter_config(tpl_display_t *display, int *visual_id, int alpha_size)
{
	if(NULL == display || TPL_TRUE != __tpl_object_is_valid(&display->base) || NULL == display->backend.filter_config)
	{
		TPL_ERR("display is invalid!");
		return TPL_FALSE;
	}

	return display->backend.filter_config(display, visual_id, alpha_size);
}


tpl_bool_t
tpl_display_get_native_window_info(tpl_display_t *display, tpl_handle_t window,
			   int *width, int *height, tbm_format *format, int depth, int a_size)
{
	if (display->backend.get_window_info == NULL)
	{
		TPL_ERR("Backend for display has not been initialized!");
                return TPL_FALSE;
	}

	return display->backend.get_window_info(display, window, width, height, format, depth, a_size);
}

tpl_bool_t
tpl_display_get_native_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
			   int *width, int *height, tbm_format *format)
{
        if (display->backend.get_pixmap_info == NULL)
	{
		TPL_ERR("Backend for display has not been initialized!");
                return TPL_FALSE;
	}

	return display->backend.get_pixmap_info(display, pixmap, width, height, format);
}

tbm_surface_h
tpl_display_get_native_pixmap_buffer(tpl_display_t *display, tpl_handle_t pixmap)
{
        if (display->backend.get_native_pixmap_buffer == NULL)
	{
		TPL_ERR("Backend for display has not been initialized!");
                return NULL;
	}

	return display->backend.get_native_pixmap_buffer(pixmap);
}
