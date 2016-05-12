#include <stdlib.h>

#include <wayland-client.h>
#include "wayland-egl.h"
#include "wayland-egl-priv.h"

#define WL_EGL_DEBUG 1
#if WL_EGL_DEBUG

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

unsigned int wl_egl_log_level;

/* WL-EGL Log Level - 0:unintialized, 1:initialized(no logging), 2:min log, 3:more log */
#define WL_EGL_LOG(lvl, f, x...)								\
	{															\
		if(wl_egl_log_level == 1)								\
		{}														\
		else if(wl_egl_log_level > 1)							\
		{														\
			if(wl_egl_log_level <= lvl)							\
				WL_EGL_LOG_PRINT(f, ##x)						\
		}														\
		else													\
		{														\
			char *env = getenv("WL_EGL_LOG_LEVEL");				\
			if(env == NULL)										\
				wl_egl_log_level = 1;							\
			else												\
				wl_egl_log_level = atoi(env);					\
																\
			if(wl_egl_log_level > 1 && wl_egl_log_level <= lvl)	\
				WL_EGL_LOG_PRINT(f, ##x)						\
		}														\
	}

#define WL_EGL_LOG_PRINT(fmt, args...)											\
	{																			\
		printf("[\x1b[32mWL-EGL\x1b[0m %d:%d|\x1b[32m%s\x1b[0m|%d] " fmt "\n",	\
			getpid(), (int)syscall(SYS_gettid), __func__,__LINE__, ##args);		\
	}

#define WL_EGL_ERR(f, x...)														\
	{																			\
		printf("[\x1b[31mWL-EGL_ERR\x1b[0m %d:%d|\x1b[31m%s\x1b[0m|%d] " f "\n",\
			getpid(), (int)syscall(SYS_gettid), __func__, __LINE__, ##x);		\
	}

#else
#define WL_EGL_LOG(lvl, f, x...)
#endif

WL_EGL_EXPORT void
wl_egl_window_resize(struct wl_egl_window *egl_window,
					 int width, int height,
					 int dx, int dy)
{
	if (egl_window == NULL) {
		WL_EGL_ERR("egl_window is NULL");
		return;
	}

	egl_window->width  = width;
	egl_window->height = height;
	egl_window->dx     = dx;
	egl_window->dy     = dy;

	if (egl_window->resize_callback)
		egl_window->resize_callback(egl_window, egl_window->private);

	WL_EGL_LOG(2, "egl_win:%10p WxH:%dx%d dx:%d dy:%d rsz_cb:%10p",
			   egl_window, egl_window->width, egl_window->height,
			   egl_window->dx, egl_window->dy, egl_window->resize_callback);
}

WL_EGL_EXPORT struct wl_egl_window *
wl_egl_window_create(struct wl_surface *surface,
					 int width, int height)
{
	struct wl_egl_window *egl_window;

	egl_window = malloc(sizeof * egl_window);
	if (!egl_window) {
		WL_EGL_ERR("failed to allocate memory for egl_window");
		return NULL;
	}

	egl_window->surface = surface;
	egl_window->private = NULL;
	egl_window->resize_callback = NULL;
	wl_egl_window_resize(egl_window, width, height, 0, 0);
	egl_window->attached_width  = 0;
	egl_window->attached_height = 0;

	WL_EGL_LOG(2, "surf:%10p WxH:%dx%d egl_win:%10p priv:%10p",
			   surface, width, height, egl_window, egl_window->private);

	return egl_window;
}

WL_EGL_EXPORT void
wl_egl_window_destroy(struct wl_egl_window *egl_window)
{
	if (egl_window == NULL) {
		WL_EGL_ERR("egl_window is NULL");
		return;
	}

	WL_EGL_LOG(2, "egl_win:%10p", egl_window);

	free(egl_window);
}

WL_EGL_EXPORT void
wl_egl_window_get_attached_size(struct wl_egl_window *egl_window,
								int *width, int *height)
{
	if (width)
		*width = egl_window->attached_width;
	if (height)
		*height = egl_window->attached_height;

	WL_EGL_LOG(2, "egl_win:%10p w:%10p h:%10p att_w:%d att_h:%d",
			   egl_window, width, height, egl_window->attached_width,
			   egl_window->attached_height);
}
