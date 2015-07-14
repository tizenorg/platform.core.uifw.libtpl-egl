#include "tpl_internal.h"

void
__tpl_display_flush(tpl_display_t *display)
{
	display->backend.flush(display);
}

static void
__tpl_display_fini(tpl_display_t *display)
{
	display->backend.fini(display);
	__tpl_runtime_remove_display(display);
}

static void
__tpl_display_free(void *display)
{
	__tpl_display_fini((tpl_display_t *)display);
	free(display);
}

tpl_display_t *
tpl_display_get(tpl_backend_type_t type, tpl_handle_t native_dpy)
{
	tpl_display_t *display;

	/* Search for an already connected display for the given native display. */
	display = __tpl_runtime_find_display(type, native_dpy);

	if (display != NULL)
		return display;

	if (type == TPL_BACKEND_UNKNOWN)
		type = __tpl_display_choose_backend(native_dpy);

	if (type == TPL_BACKEND_UNKNOWN)
		return NULL;

	display = (tpl_display_t *)calloc(1, sizeof(tpl_display_t));
	TPL_ASSERT(display != NULL);

	/* Initialize object base class. */
	__tpl_object_init(&display->base, TPL_OBJECT_DISPLAY, __tpl_display_free);

	/* Initialize display object. */
	display->native_handle = native_dpy;

	/* Initialize backend. */
	__tpl_display_init_backend(display, type);

	if (!display->backend.init(display))
	{
		tpl_object_unreference((tpl_object_t *)display);
		return NULL;
	}

	/* Add it to the runtime. */
	__tpl_runtime_add_display(display);

	return display;
}

tpl_bool_t
tpl_display_bind_client_display_handle(tpl_display_t *display, tpl_handle_t native_dpy)
{
	if (display->backend.bind_client_display_handle != NULL)
		return display->backend.bind_client_display_handle(display, native_dpy);
	return TPL_FALSE;
}

tpl_bool_t
tpl_display_unbind_client_display_handle(tpl_display_t *display, tpl_handle_t native_dpy)
{
	if (display->backend.unbind_client_display_handle != NULL)
		return display->backend.unbind_client_display_handle(display, native_dpy);
	return TPL_FALSE;
}

tpl_backend_type_t
tpl_display_get_backend_type(tpl_display_t *display)
{
	TPL_ASSERT(__tpl_object_is_valid(&display->base));
	return display->backend.type;
}

int
tpl_display_get_bufmgr_fd(tpl_display_t *display)
{
	TPL_ASSERT(__tpl_object_is_valid(&display->base));
	return display->bufmgr_fd;
}

tpl_handle_t
tpl_display_get_native_handle(tpl_display_t *display)
{
	TPL_ASSERT(__tpl_object_is_valid(&display->base));
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
	TPL_ASSERT(__tpl_object_is_valid(&display->base));
	return display->backend.query_config(display,
					      surface_type,
					      red_size, green_size, blue_size, alpha_size,
					      depth_size, native_visual_id, is_slow);
}

tpl_bool_t
tpl_display_filter_config(tpl_display_t *display,
			int *visual_id,
			int alpha_size)
{
	TPL_ASSERT(__tpl_object_is_valid(&display->base));
	if(display->backend.filter_config != NULL)
		return display->backend.filter_config(display, visual_id, alpha_size);

	return TPL_FALSE;
}

void
tpl_display_flush(tpl_display_t *display)
{
	if (display == NULL)
		__tpl_runtime_flush_all_display();
	else
	{
		TPL_OBJECT_LOCK(display);
		__tpl_display_flush(display);
		TPL_OBJECT_UNLOCK(display);
	}
}

void
tpl_display_wait_native(tpl_display_t *dpy)
{
	TPL_ASSERT(dpy != NULL);

	dpy->backend.wait_native(dpy);
}