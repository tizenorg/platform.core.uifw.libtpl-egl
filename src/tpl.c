#include "tpl_internal.h"

unsigned int tpl_log_lvl;

struct _tpl_runtime
{
	tpl_hlist_t *displays[TPL_BACKEND_COUNT];
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
				tpl_hashlist_destroy(&(runtime->displays[i]));
		}

		free(runtime);
		runtime = NULL;
	}
}

/* Begin: OS dependent function definition */
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

int tpl_util_atomic_get(const tpl_util_atomic_uint * const atom)
{
	unsigned int ret;

	if (!atom)
		abort();

	TPL_DMB();
	ret = *atom;
	TPL_DMB();

	return ret;
}

void tpl_util_atomic_set(tpl_util_atomic_uint * const atom, unsigned int val)
{
	if (!atom)
		abort();

	TPL_DMB();
	*atom = val;
	TPL_DMB();
}

int tpl_util_atomic_inc(tpl_util_atomic_uint * const atom )
{
	if (!atom)
		abort();

	return __sync_add_and_fetch(atom, 1);
}
int tpl_util_atomic_dec( tpl_util_atomic_uint * const atom )
{
	if (!atom)
		abort();

	return __sync_sub_and_fetch(atom, 1);
}
/* End: OS dependent function definition */

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
			display = (tpl_display_t *) tpl_hashlist_lookup(runtime->displays[type],
									(size_t) native_display);
		}
	}
	else
	{
		int i;

		for (i = 0; i < TPL_BACKEND_COUNT; i++)
		{
			if (runtime->displays[i] != NULL)
			{
				display = (tpl_display_t *) tpl_hashlist_lookup(runtime->displays[i],
										(size_t) native_display);
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
			runtime->displays[type] = tpl_hashlist_create();

		ret = tpl_hashlist_insert(runtime->displays[type], (size_t) handle, (void *) display);
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
			tpl_hashlist_delete(runtime->displays[type], (size_t) handle);
	}

	pthread_mutex_unlock(&runtime_mutex);
}

void __tpl_runtime_flush_cb(void *data)
{
	TPL_OBJECT_LOCK(data);
	__tpl_display_flush(data);
	TPL_OBJECT_UNLOCK(data);
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
			tpl_hashlist_do_for_all_nodes(runtime->displays[i],
						      __tpl_runtime_flush_cb);
	}

	pthread_mutex_unlock(&runtime_mutex);
}

tpl_backend_type_t
__tpl_display_choose_backend(tpl_handle_t native_dpy)
{
#ifdef TPL_WINSYS_WL
	if (__tpl_display_choose_backend_wayland(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_WAYLAND;
#endif
#ifdef TPL_WINSYS_DRI2
	if (__tpl_display_choose_backend_x11_dri2(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_X11_DRI2;
#endif
#ifdef TPL_WINSYS_DRI3
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
#ifdef TPL_WINSYS_WL
	case TPL_BACKEND_WAYLAND:
		__tpl_display_init_backend_wayland(&display->backend);
		break;
#endif
#ifdef TPL_WINSYS_DRI2
	case TPL_BACKEND_X11_DRI2:
		__tpl_display_init_backend_x11_dri2(&display->backend);
		break;
#endif
#ifdef TPL_WINSYS_DRI3
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
#ifdef TPL_WINSYS_WL
	case TPL_BACKEND_WAYLAND:
		__tpl_surface_init_backend_wayland(&surface->backend);
		break;
#endif
#ifdef TPL_WINSYS_DRI2
	case TPL_BACKEND_X11_DRI2:
		__tpl_surface_init_backend_x11_dri2(&surface->backend);
		break;
#endif
#ifdef TPL_WINSYS_DRI3
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
#ifdef TPL_WINSYS_WL
	case TPL_BACKEND_WAYLAND:
		__tpl_buffer_init_backend_wayland(&buffer->backend);
		break;
#endif
#ifdef TPL_WINSYS_DRI2
	case TPL_BACKEND_X11_DRI2:
		__tpl_buffer_init_backend_x11_dri2(&buffer->backend);
		break;
#endif
#ifdef TPL_WINSYS_DRI3
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
