#include "tpl_internal.h"

unsigned int tpl_log_lvl;

struct _tpl_runtime
{
	tpl_egl_funcs_t *egl_funcs;
	tpl_utils_ptrdict displays[TPL_BACKEND_COUNT];
};

static tpl_runtime_t	*runtime = NULL;
static pthread_mutex_t	runtime_mutex = PTHREAD_MUTEX_INITIALIZER;

static void
__tpl_runtime_init()
{
	if (runtime == NULL)
	{
		runtime = (tpl_runtime_t *)calloc(1, sizeof(tpl_runtime_t));
		TPL_ASSERT(runtime != NULL);
	}
}

static void __attribute__((destructor))
__tpl_runtime_fini()
{
	if (runtime != NULL)
	{
		int i;

		for (i = 0; i < TPL_BACKEND_COUNT; i++)
		{
			if (runtime->displays[i] != NULL)
				tpl_utils_ptrdict_free(runtime->displays[i]);
		}

		free(runtime);
		runtime = NULL;
	}
}

void tpl_set_egl_funcs(tpl_egl_funcs_t *eglfuncs)
{
	__tpl_runtime_init();

	runtime->egl_funcs = eglfuncs;
}

/* Begin: DDK dependent types and function definition */
void tpl_util_sys_yield(void)
{
	int status;
	status = sched_yield();
	if (0 != status)
	{
		/* non-fatal on error, warning is enough */
		TPL_WARN("Yield failed, ret=%.8x\n", status);
	}
}

int tpl_util_clz(int val)
{
	return __builtin_clz( val );
}

int tpl_util_osu_atomic_get(const tpl_util_osu_atomic * const atom)
{
	return runtime->egl_funcs->atomic_get(atom);
}

void tpl_util_osu_atomic_set(tpl_util_osu_atomic * const atom, int val)
{
	runtime->egl_funcs->atomic_set(atom, val);
}

int tpl_util_osu_atomic_inc( tpl_util_osu_atomic * const atom )
{
	return 	runtime->egl_funcs->atomic_inc(atom);
}
int tpl_util_osu_atomic_dec( tpl_util_osu_atomic * const atom )
{
	return runtime->egl_funcs->atomic_dec(atom);
}

tpl_utils_ptrdict tpl_utils_ptrdict_allocate(void (*freefunc)(void *))
{
	tpl_utils_ptrdict d;
	d = malloc(runtime->egl_funcs->ptrdict_size);

	if (!d)
		return NULL;

	runtime->egl_funcs->ptrdict_init(d, NULL, NULL, freefunc);
	return d;
}

tpl_bool_t tpl_utils_ptrdict_insert(tpl_utils_ptrdict d, void *name, void *data)
{
	return (tpl_bool_t) runtime->egl_funcs->ptrdict_insert(d, name, data);
}


void *tpl_utils_ptrdict_get(tpl_utils_ptrdict d, void *name)
{
	void *ret;
	runtime->egl_funcs->ptrdict_lookup_key(d, name, &ret);
	return ret;
}

void tpl_utils_ptrdict_free(tpl_utils_ptrdict d)
{
	runtime->egl_funcs->ptrdict_term(d);
}

void tpl_utils_ptrdict_remove(tpl_utils_ptrdict d, void *name)
{
	runtime->egl_funcs->ptrdict_remove(d, name);
}

void tpl_utils_ptrdict_iterate_init(tpl_utils_ptrdict d, tpl_utils_ptrdict_iter it)
{
	runtime->egl_funcs->ptrdict_iter_init(it, d);
}

void *tpl_utils_ptrdict_next( tpl_utils_ptrdict_iter it, void  **value )
{
	return runtime->egl_funcs->ptrdict_next(it, value);
}
/* End: DDK dependent types and function definition */

tpl_display_t *
__tpl_runtime_find_display(tpl_backend_type_t type, tpl_handle_t native_display)
{
	tpl_display_t *display = NULL;

	if (runtime == NULL)
		return NULL;

	pthread_mutex_lock(&runtime_mutex);

	if (type != TPL_BACKEND_UNKNOWN)
	{
		if (runtime->displays[type] != NULL)
		{
			display = (tpl_display_t *) tpl_utils_ptrdict_get(runtime->displays[type],
									  (void *) native_display);
		}
	}
	else
	{
		int i;

		for (i = 0; i < TPL_BACKEND_COUNT; i++)
		{
			if (runtime->displays[i] != NULL)
			{
				display = (tpl_display_t *) tpl_utils_ptrdict_get(runtime->displays[i],
										  (void *) native_display);
			}
			if (display != NULL) break;
		}
	}

	pthread_mutex_unlock(&runtime_mutex);

	return display;
}

void
__tpl_runtime_add_display(tpl_display_t *display)
{
	tpl_bool_t ret;
	tpl_handle_t handle = display->native_handle;
	tpl_backend_type_t type = display->backend.type;

	pthread_mutex_lock(&runtime_mutex);
	__tpl_runtime_init();

	if (type != TPL_BACKEND_UNKNOWN)
	{
		if (runtime->displays[type] == NULL)
			runtime->displays[type] = tpl_utils_ptrdict_allocate(NULL);

		ret = tpl_utils_ptrdict_insert(runtime->displays[type], (void *) handle, (void *)display);
		TPL_ASSERT(ret == TPL_TRUE);
	}

	pthread_mutex_unlock(&runtime_mutex);
}

void
__tpl_runtime_remove_display(tpl_display_t *display)
{
	tpl_handle_t handle = display->native_handle;
	tpl_backend_type_t type = display->backend.type;

	pthread_mutex_lock(&runtime_mutex);

	if (type != TPL_BACKEND_UNKNOWN)
	{
		if (runtime != NULL && runtime->displays[type] != NULL)
			tpl_utils_ptrdict_remove(runtime->displays[type], (void *) handle);
	}

	pthread_mutex_unlock(&runtime_mutex);
}

void
__tpl_runtime_flush_all_display()
{
	int i;

	if (runtime == NULL)
		return;

	pthread_mutex_lock(&runtime_mutex);

	for (i = 0; i < TPL_BACKEND_COUNT; i++)
	{
		if (runtime->displays[i] != NULL)
		{
			tpl_utils_ptrdict_iter iterator;
			tpl_display_t *display;

			tpl_utils_ptrdict_iterate_init(runtime->displays[i], &iterator);

			while (tpl_utils_ptrdict_next( &iterator, (void **)(&display)))
			{
				TPL_OBJECT_LOCK(display);
				__tpl_display_flush(display);
				TPL_OBJECT_UNLOCK(display);
			}
		}
	}

	pthread_mutex_unlock(&runtime_mutex);
}

tpl_backend_type_t
__tpl_display_choose_backend(tpl_handle_t native_dpy)
{
#if TPL_WITH_WAYLAND == 1
	if (__tpl_display_choose_backend_wayland(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_WAYLAND;
#endif
#if TPL_WITH_X11_DRI2 == 1
	if (__tpl_display_choose_backend_x11_dri2(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_X11_DRI2;
#endif
#if TPL_WITH_X11_DRI3 == 1
	if (__tpl_display_choose_backend_x11_dri3(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_X11_DRI3;
#endif
	return TPL_BACKEND_UNKNOWN;
}

void
__tpl_display_init_backend(tpl_display_t *display, tpl_backend_type_t type)
{
	switch (type)
	{
#if TPL_WITH_WAYLAND == 1
	case TPL_BACKEND_WAYLAND:
		__tpl_display_init_backend_wayland(&display->backend);
		break;
#endif
#if TPL_WITH_X11_DRI2 == 1
	case TPL_BACKEND_X11_DRI2:
		__tpl_display_init_backend_x11_dri2(&display->backend);
		break;
#endif
#if TPL_WITH_X11_DRI3 == 1
	case TPL_BACKEND_X11_DRI3:
		__tpl_display_init_backend_x11_dri3(&display->backend);
		break;
#endif
	default:
		TPL_ASSERT(TPL_FALSE);
		break;
	}
}

void
__tpl_surface_init_backend(tpl_surface_t *surface, tpl_backend_type_t type)
{
	switch (type)
	{
#if TPL_WITH_WAYLAND == 1
	case TPL_BACKEND_WAYLAND:
		__tpl_surface_init_backend_wayland(&surface->backend);
		break;
#endif
#if TPL_WITH_X11_DRI2 == 1
	case TPL_BACKEND_X11_DRI2:
		__tpl_surface_init_backend_x11_dri2(&surface->backend);
		break;
#endif
#if TPL_WITH_X11_DRI3 == 1
	case TPL_BACKEND_X11_DRI3:
		__tpl_surface_init_backend_x11_dri3(&surface->backend);
		break;
#endif
	default:
		TPL_ASSERT(TPL_FALSE);
		break;
	}
}

void
__tpl_buffer_init_backend(tpl_buffer_t *buffer, tpl_backend_type_t type)
{
	switch (type)
	{
#if TPL_WITH_WAYLAND == 1
	case TPL_BACKEND_WAYLAND:
		__tpl_buffer_init_backend_wayland(&buffer->backend);
		break;
#endif
#if TPL_WITH_X11_DRI2 == 1
	case TPL_BACKEND_X11_DRI2:
		__tpl_buffer_init_backend_x11_dri2(&buffer->backend);
		break;
#endif
#if TPL_WITH_X11_DRI3 == 1
	case TPL_BACKEND_X11_DRI3:
		__tpl_buffer_init_backend_x11_dri3(&buffer->backend);
		break;
#endif
	default:
		TPL_ASSERT(TPL_FALSE);
		break;
	}
}

tpl_bool_t
tpl_get_native_window_info(tpl_display_t *display, tpl_handle_t window,
			   int *width, int *height, tpl_format_t *format, int depth, int a_size)
{
	return display->backend.get_window_info(display, window, width, height, format, depth, a_size);
}

tpl_bool_t
tpl_get_native_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
			   int *width, int *height, tpl_format_t *format)
{
	return display->backend.get_pixmap_info(display, pixmap, width, height, format);
}
