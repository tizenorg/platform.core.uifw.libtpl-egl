#include "tpl_internal.h"

unsigned int tpl_log_lvl;
unsigned int tpl_dump_lvl;

struct _tpl_runtime {
	tpl_hlist_t *displays[TPL_BACKEND_COUNT];
};

static tpl_runtime_t	*runtime = NULL;
static pthread_mutex_t	runtime_mutex = PTHREAD_MUTEX_INITIALIZER;

static tpl_result_t
__tpl_runtime_init()
{
	if (runtime == NULL) {
		runtime = (tpl_runtime_t *) calloc(1, sizeof(tpl_runtime_t));
		if (runtime == NULL) {
			TPL_ERR("Failed to allocate new tpl_runtime_t.");
			return TPL_ERROR_INVALID_OPERATION;
		}
	}

	return TPL_ERROR_NONE;
}

static void __attribute__((destructor))
__tpl_runtime_fini()
{
	if (runtime != NULL) {
		int i;

		for (i = 0; i < TPL_BACKEND_COUNT; i++) {
			if (runtime->displays[i] != NULL)
				__tpl_hashlist_destroy(&(runtime->displays[i]));
		}

		free(runtime);
		runtime = NULL;
	}
}

/* Begin: OS dependent function definition */
void
__tpl_util_sys_yield(void)
{
	int status;

	status = sched_yield();

	if (status != 0) {
		/* non-fatal on error, warning is enough */
		TPL_WARN("Yield failed, ret=%.8x\n", status);
	}
}

int
__tpl_util_clz(int val)
{
	return __builtin_clz( val );
}

int
__tpl_util_atomic_get(const tpl_util_atomic_uint *const atom)
{
	unsigned int ret;

	TPL_ASSERT(atom);

	TPL_DMB();
	ret = *atom;
	TPL_DMB();

	return ret;
}

void
__tpl_util_atomic_set(tpl_util_atomic_uint *const atom, unsigned int val)
{
	TPL_ASSERT(atom);

	TPL_DMB();
	*atom = val;
	TPL_DMB();
}

unsigned int
__tpl_util_atomic_inc(tpl_util_atomic_uint *const atom )
{
	TPL_ASSERT(atom);

	return __sync_add_and_fetch(atom, 1);
}

unsigned int
__tpl_util_atomic_dec( tpl_util_atomic_uint *const atom )
{
	TPL_ASSERT(atom);

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

	if (type != TPL_BACKEND_UNKNOWN) {
		if (runtime->displays[type] != NULL) {
			display = (tpl_display_t *) __tpl_hashlist_lookup(runtime->displays[type],
					  (size_t) native_display);
		}
	} else {
		int i;

		for (i = 0; i < TPL_BACKEND_COUNT; i++) {
			if (runtime->displays[i] != NULL) {
				display = (tpl_display_t *) __tpl_hashlist_lookup(runtime->displays[i],
						  (size_t) native_display);
			}
			if (display != NULL) break;
		}
	}

	pthread_mutex_unlock(&runtime_mutex);

	return display;
}

tpl_result_t
__tpl_runtime_add_display(tpl_display_t *display)
{
	tpl_result_t ret;
	tpl_handle_t handle;
	tpl_backend_type_t type;

	TPL_ASSERT(display);

	handle = display->native_handle;
	type = display->backend.type;

	TPL_ASSERT(0 <= type && TPL_BACKEND_COUNT > type);

	if (0 != pthread_mutex_lock(&runtime_mutex)) {
		TPL_ERR("runtime_mutex pthread_mutex_lock failed.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	if (TPL_ERROR_NONE != __tpl_runtime_init()) {
		TPL_ERR("__tpl_runtime_init() failed.");
		pthread_mutex_unlock(&runtime_mutex);
		return TPL_ERROR_INVALID_OPERATION;
	}

	if (NULL == runtime->displays[type]) {
		runtime->displays[type] = __tpl_hashlist_create();
		if (NULL == runtime->displays[type]) {
			TPL_ERR("__tpl_hashlist_create failed.");
			pthread_mutex_unlock(&runtime_mutex);
			return TPL_ERROR_INVALID_OPERATION;
		}
	}

	ret = __tpl_hashlist_insert(runtime->displays[type],
								(size_t) handle, (void *) display);
	if (TPL_ERROR_NONE != ret) {
		TPL_ERR("__tpl_hashlist_insert failed. list(%p), handle(%d), display(%p)",
				runtime->displays[type], handle, display);
		pthread_mutex_unlock(&runtime_mutex);
		return TPL_ERROR_INVALID_OPERATION;
	}

	pthread_mutex_unlock(&runtime_mutex);

	return TPL_ERROR_NONE;
}

void
__tpl_runtime_remove_display(tpl_display_t *display)
{
	tpl_handle_t handle = display->native_handle;
	tpl_backend_type_t type = display->backend.type;

	pthread_mutex_lock(&runtime_mutex);

	if (type != TPL_BACKEND_UNKNOWN) {
		if (runtime != NULL && runtime->displays[type] != NULL)
			__tpl_hashlist_delete(runtime->displays[type],
								  (size_t) handle);
	}

	pthread_mutex_unlock(&runtime_mutex);
}

tpl_backend_type_t
__tpl_display_choose_backend(tpl_handle_t native_dpy)
{
	const char *plat_name = NULL;
	plat_name = getenv("EGL_PLATFORM");

	if (plat_name) {
#ifdef TPL_WINSYS_DRI2
		if (strcmp(plat_name, "x11") == 0) return TPL_BACKEND_X11_DRI2;
#endif
#ifdef TPL_WINSYS_DRI3
		if (strcmp(plat_name, "x11") == 0) return TPL_BACKEND_X11_DRI3;
#endif
#ifdef TPL_WINSYS_WL
		if (strcmp(plat_name, "tbm") == 0) return TPL_BACKEND_TBM;
		if (strcmp(plat_name, "wayland") == 0) return TPL_BACKEND_WAYLAND;
		if (strcmp(plat_name, "wayland_vulkan_wsi") == 0) return
				TPL_BACKEND_WAYLAND_VULKAN_WSI;
		if (strcmp(plat_name, "drm") == 0) return TPL_BACKEND_GBM;
#endif
	}

#ifdef TPL_WINSYS_WL
	if (__tpl_display_choose_backend_tbm(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_TBM;
	if (__tpl_display_choose_backend_gbm(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_GBM;
	if (__tpl_display_choose_backend_wayland_egl(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_WAYLAND;
	if (__tpl_display_choose_backend_wayland_vk_wsi(native_dpy) == TPL_TRUE)
		return TPL_BACKEND_WAYLAND_VULKAN_WSI;
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
	TPL_ASSERT(display);
	TPL_ASSERT(0 <= type && TPL_BACKEND_COUNT > type);

	switch (type) {
#ifdef TPL_WINSYS_WL
	case TPL_BACKEND_GBM:
		__tpl_display_init_backend_gbm(&display->backend);
		break;
	case TPL_BACKEND_WAYLAND:
		__tpl_display_init_backend_wayland_egl(&display->backend);
		break;
	case TPL_BACKEND_WAYLAND_VULKAN_WSI:
		__tpl_display_init_backend_wayland_vk_wsi(&display->backend);
		break;
	case TPL_BACKEND_TBM:
		__tpl_display_init_backend_tbm(&display->backend);
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
	}
}

void
__tpl_surface_init_backend(tpl_surface_t *surface, tpl_backend_type_t type)
{
	TPL_ASSERT(surface);
	TPL_ASSERT(0 <= type && TPL_BACKEND_COUNT > type);

	switch (type) {
#ifdef TPL_WINSYS_WL
	case TPL_BACKEND_GBM:
		__tpl_surface_init_backend_gbm(&surface->backend);
		break;
	case TPL_BACKEND_WAYLAND:
		__tpl_surface_init_backend_wayland_egl(&surface->backend);
		break;
	case TPL_BACKEND_WAYLAND_VULKAN_WSI:
		__tpl_surface_init_backend_wayland_vk_wsi(&surface->backend);
		break;
	case TPL_BACKEND_TBM:
		__tpl_surface_init_backend_tbm(&surface->backend);
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
	}
}

