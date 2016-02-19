#ifndef TPL_X11_INTERNAL_H
#define TPL_X11_INTERNAL_H

#include "tpl.h"
#include <stdlib.h>
#include <pthread.h>

#include "tpl_utils.h"

#define TIZEN_FEATURES_ENABLE			0

#define DRI2_BUFFER_FB                   0x02
#define DRI2_BUFFER_MAPPED               0x04
#define DRI2_BUFFER_REUSED               0x08
#define DRI2_BUFFER_AGE                  0x70 /* 01110000 */

#define DRI2_BUFFER_IS_FB(flag)         ((flag & DRI2_BUFFER_FB) ? 1 : 0)
#define DRI2_BUFFER_IS_REUSED(flag)	((flag & DRI2_BUFFER_REUSED) ? 1 : 0)
#define DRI2_BUFFER_GET_AGE(flag)	((flag & DRI2_BUFFER_AGE) >> 4)

#define TPL_STACK_XRECTANGLE_SIZE	16
/* [BEGIN: 20141125-xuelian.bai] DRI3 need lots of buffer cache. or it will get
 * slow */
#define TPL_BUFFER_CACHE_MAX_ENTRIES	40
/* [END: 20141125-xuelian.bai] */

#define EGL_X11_WINDOW_SWAP_TYPE_ENV_NAME	"EGL_X11_SWAP_TYPE_WINDOW"
#define EGL_X11_FB_SWAP_TYPE_ENV_NAME		"EGL_X11_SWAP_TYPE_FB"

typedef struct _tpl_x11_global	tpl_x11_global_t;

typedef enum {
	TPL_X11_SWAP_TYPE_ERROR = -1,
	TPL_X11_SWAP_TYPE_SYNC = 0,
	TPL_X11_SWAP_TYPE_ASYNC,
	TPL_X11_SWAP_TYPE_LAZY,
	TPL_X11_SWAP_TYPE_MAX
} tpl_x11_swap_type_t;

struct _tpl_x11_global {
	int		display_count;

	Display		*worker_display;
	int		bufmgr_fd;
	tbm_bufmgr	bufmgr;

	tpl_x11_swap_type_t	win_swap_type;
	tpl_x11_swap_type_t	fb_swap_type;
};

pthread_mutex_t
__tpl_x11_get_global_mutex(void);

void
__tpl_x11_swap_str_to_swap_type(char *str, tpl_x11_swap_type_t *type);

tpl_buffer_t *
__tpl_x11_surface_buffer_cache_find(tpl_list_t	 *buffer_cache,
				    unsigned int name);
void
__tpl_x11_surface_buffer_cache_remove(tpl_list_t 	*buffer_cache,
				      unsigned int name);
tpl_bool_t
__tpl_x11_surface_buffer_cache_add(tpl_list_t	*buffer_cache,
				   tpl_buffer_t *buffer);
void
__tpl_x11_surface_buffer_cache_clear(tpl_list_t	*buffer_cache);
tpl_bool_t
__tpl_x11_display_query_config(tpl_display_t *display,
			       tpl_surface_type_t surface_type, int red_size,
			       int green_size, int blue_size, int alpha_size,
			       int color_depth, int *native_visual_id, tpl_bool_t *is_slow);
tpl_bool_t
__tpl_x11_display_get_window_info(tpl_display_t *display, tpl_handle_t window,
				  int *width, int *height, tpl_format_t *format, int depth, int a_size);
tpl_bool_t
__tpl_x11_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
				  int *width, int *height, tpl_format_t *format);
void
__tpl_x11_display_flush(tpl_display_t *display);
tpl_bool_t
__tpl_x11_buffer_init(tpl_buffer_t *buffer);
void
__tpl_x11_buffer_fini(tpl_buffer_t *buffer);
void *
__tpl_x11_buffer_map(tpl_buffer_t *buffer, int size);
void
__tpl_x11_buffer_unmap(tpl_buffer_t *buffer, void *ptr, int size);
tpl_bool_t
__tpl_x11_buffer_lock(tpl_buffer_t *buffer, tpl_lock_usage_t usage);
void
__tpl_x11_buffer_unlock(tpl_buffer_t *buffer);
tpl_bool_t __tpl_x11_buffer_get_reused_flag(tpl_buffer_t *buffer);
void
__tpl_x11_display_wait_native(tpl_display_t *display);

#endif /* TPL_X11_INTERNAL_H */
