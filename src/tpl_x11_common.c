#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include <libdrm/drm.h>
#include <xf86drm.h>

#include <dri2/dri2.h>
#include <tbm_bufmgr.h>

#include "tpl_internal.h"

#include "tpl_x11_internal.h"

static pthread_mutex_t	global_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t
__tpl_x11_get_global_mutex()
{
	return global_mutex;
}

void
__tpl_x11_swap_str_to_swap_type(char *str, tpl_x11_swap_type_t *type)
{
	int swap_type;

	if (str == NULL)
		return;

	swap_type = strtol(str, NULL, 0);

	switch (swap_type)
	{
	case TPL_X11_SWAP_TYPE_SYNC:
	case TPL_X11_SWAP_TYPE_ASYNC:
	case TPL_X11_SWAP_TYPE_LAZY:
		*type = swap_type;
		break;
	default:
		break;
	}
}

tpl_buffer_t *
__tpl_x11_surface_buffer_cache_find(tpl_list_t	 *buffer_cache, unsigned int name)
{
	tpl_list_node_t *node = tpl_list_get_front_node(buffer_cache);

	while (node)
	{
		tpl_buffer_t *buffer = (tpl_buffer_t *)tpl_list_node_get_data(node);

		if (buffer->key == name)
			return buffer;

		node = tpl_list_node_next(node);
	}

	return NULL;
}

void
__tpl_x11_surface_buffer_cache_remove(tpl_list_t	 *buffer_cache, unsigned int name)
{
	tpl_list_node_t *node = tpl_list_get_front_node(buffer_cache);

	while (node)
	{
		tpl_buffer_t *buffer = (tpl_buffer_t *)tpl_list_node_get_data(node);

		if (buffer->key == name)
		{
			tpl_object_unreference(&buffer->base);
			tpl_list_remove(node, NULL);
			return;
		}

		node = tpl_list_node_next(node);
	}
}

void
__tpl_x11_surface_buffer_cache_add(tpl_list_t *buffer_cache, tpl_buffer_t *buffer)
{
	if (tpl_list_get_count(buffer_cache) >= TPL_BUFFER_CACHE_MAX_ENTRIES)
	{
		tpl_buffer_t *evict = tpl_list_pop_front(buffer_cache, NULL);
		tpl_object_unreference(&evict->base);
	}

	tpl_object_reference(&buffer->base);
	tpl_list_push_back(buffer_cache, (void *)buffer);
}

void
__tpl_x11_surface_buffer_cache_clear(tpl_list_t *buffer_cache)
{
	tpl_list_fini(buffer_cache, (tpl_free_func_t)tpl_object_unreference);
}


tpl_bool_t
__tpl_x11_display_query_config(tpl_display_t *display,
			       tpl_surface_type_t surface_type, int red_size,
			       int green_size, int blue_size, int alpha_size,
			       int color_depth, int *native_visual_id, tpl_bool_t *is_slow)
{
	Display *native_display;

    TPL_IGNORE(alpha_size);

	native_display = (Display *)display->native_handle;

	if (red_size != TPL_DONT_CARE || green_size != TPL_DONT_CARE ||
	    blue_size != TPL_DONT_CARE || color_depth != TPL_DONT_CARE)
	{
		if (surface_type == TPL_SURFACE_TYPE_WINDOW)
		{
			XVisualInfo *visual_formats;
			int num_visual_formats;
			int i;

			visual_formats = XGetVisualInfo(native_display, 0, NULL,
							&num_visual_formats);
			TPL_ASSERT(visual_formats);
			for (i = 0; i < num_visual_formats; i++)
			{
				int clz[3];
				int col_size[3];

				clz[0] = tpl_util_clz(visual_formats[i].red_mask);
				clz[1] = tpl_util_clz(visual_formats[i].green_mask);
				clz[2] = tpl_util_clz(visual_formats[i].blue_mask);

				col_size[0] = clz[1] - clz[0];
				col_size[1] = clz[2] - clz[1];
				col_size[2] = 32 - clz[2];

				if ((red_size == TPL_DONT_CARE || col_size[0] == red_size) &&
				    (green_size == TPL_DONT_CARE || col_size[1] == green_size) &&
				    (blue_size == TPL_DONT_CARE || col_size[2] == blue_size))
				{
					if (native_visual_id != NULL)
						*native_visual_id = visual_formats[i].visualid;

					if (is_slow != NULL)
						*is_slow = TPL_FALSE;

					return TPL_TRUE;
				}
			}
			XFree(visual_formats);
			visual_formats = NULL;
		}

		if (surface_type == TPL_SURFACE_TYPE_PIXMAP)
		{
			XPixmapFormatValues *pixmap_formats;
			int num_pixmap_formats;
			int i;

			pixmap_formats = XListPixmapFormats(native_display, &num_pixmap_formats);
			TPL_ASSERT(pixmap_formats);
			for (i = 0; i < num_pixmap_formats; i++)
			{
				if (color_depth == TPL_DONT_CARE ||
				    pixmap_formats[i].depth == color_depth)
				{
					if (is_slow != NULL)
						*is_slow = TPL_FALSE;

					return TPL_TRUE;
				}
			}
			XFree(pixmap_formats);
			pixmap_formats = NULL;
		}

		return TPL_FALSE;

	}

	return TPL_TRUE;
}

#if 0
static void tpl_handle_and_free_error( Display *dpy, xcb_generic_error_t *error, const char* request_string )
{
	char error_txt[256];

	if( error )
	{
		int len = sizeof(error_txt)/sizeof(error_txt[0]);

		XGetErrorText( dpy, error->error_code, error_txt, len );
		error_txt[ len - 1] = '\0';
		TPL_WARN("%s failed \"[%d]:%s\"", request_string, error->error_code, error_txt );
		free(error);
	}
	else
	{
		TPL_WARN("%s failed \"Unknown error\"", request_string );
	}
}

static tpl_bool_t tpl_check_reply_for_error(Display *dpy, xcb_generic_reply_t *reply, xcb_generic_error_t *error,
		const char *request_string)
{
	tpl_bool_t retval = TPL_FALSE;

	if (error || reply == NULL)
	{
		tpl_handle_and_free_error( dpy, error, request_string );
	}
	else
	{
		retval = TPL_TRUE;
	}

	return retval;
}
static XVisualInfo* tpl_find_visual( Display *dpy, xcb_visualid_t visual_id )
{
	XVisualInfo *visual_info;
	XVisualInfo visual_info_template;
	int matching_count;

	visual_info_template.visualid = visual_id;

	visual_info = XGetVisualInfo(dpy, VisualIDMask, &visual_info_template, &matching_count);


	return visual_info;
}
static int tpl_get_alpha_offset( int offset_r, int offset_g, int offset_b, int bpp )
{
	int ret = -1;

	TPL_CHECK_ON_FALSE_ASSERT_FAIL( bpp == 32, "alpha only supported for 32bits pixel formats");

	if( offset_r != 0 && offset_g != 0 && offset_b != 0 )
	{
		ret = 0;
	}
	else if( offset_r != 24 && offset_g != 24 && offset_b != 24 )
	{
		ret = 24;
	}
	else
	{
		TPL_CHECK_ON_FALSE_ASSERT_FAIL(TPL_FALSE, "Alpha component has to be at either the offset 0 or 24");
	}

	return ret;
}
static int tpl_get_offset( unsigned long mask, int depth )
{
	int res = -1;
	int count;

	for (count = 0; count < depth; count++)
	{
		if (mask & 1)
		{
			res = count;
			break;
		}
		mask = mask >> 1;
	}

	return res;
}
/* Convert the given combination of offsets and bpp into a color buffer format */
static tpl_format_t tpl_offsets_to_color_buffer_format( int offset_r, int offset_g, int offset_b, int offset_a, int bpp )
{
	tpl_format_t retval = TPL_FORMAT_INVALID;

	if ( offset_b == 11 && offset_g == 5  && offset_r == 0  && offset_a == -1 && bpp == 16)
	{
		retval = TPL_FORMAT_BGR565;
	}
	else if( offset_r == 11 && offset_g == 5  && offset_b == 0  && offset_a == -1 && bpp == 16)
	{
		retval = TPL_FORMAT_RGB565;
	}

	else if( offset_a == 24 && offset_b == 16 && offset_g == 8  && offset_r == 0  && bpp == 32)
	{
		retval = TPL_FORMAT_ABGR8888;
	}
	else if( offset_a == 24 && offset_r == 16 && offset_g == 8  && offset_b == 0  && bpp == 32)
	{
		retval = TPL_FORMAT_ARGB8888;
	}
	else if( offset_b == 24 && offset_g == 16 && offset_r == 8  && offset_a == 0  && bpp == 32)
	{
		retval = TPL_FORMAT_BGRA8888;
	}
	else if( offset_r == 24 && offset_g == 16 && offset_b == 8  && offset_a == 0  && bpp == 32)
	{
		retval = TPL_FORMAT_RGBA8888;
	}

	else if( offset_b == 16 && offset_g == 8  && offset_r == 0  && offset_a == -1 && bpp == 32)
	{
		retval = TPL_FORMAT_XBGR8888;
	}
	else if( offset_r == 16 && offset_g == 8  && offset_b == 0  && offset_a == -1 && bpp == 32)
	{
		retval = TPL_FORMAT_XRGB8888;
	}
	else if( offset_b == 24 && offset_g == 16 && offset_r == 8  && offset_a == -1 && bpp == 32)
	{
		retval = TPL_FORMAT_BGRX8888;
	}
	else if( offset_r == 24 && offset_g == 16 && offset_b == 8  && offset_a == -1 && bpp == 32)
	{
		retval = TPL_FORMAT_RGBX8888;
	}

	else if( offset_b == 16 && offset_g == 8  && offset_r == 0  && offset_a == -1 && bpp == 24)
	{
		retval = TPL_FORMAT_BGR888;
	}
	else if( offset_r == 16 && offset_g == 8  && offset_b == 0  && offset_a == -1 && bpp == 24)
	{
		retval = TPL_FORMAT_RGB888;
	}

	else if( offset_a == 12 && offset_b == 8  && offset_g == 4  && offset_r == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_ABGR4444;
	}
	else if( offset_a == 12 && offset_r == 8  && offset_g == 4  && offset_b == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_ARGB4444;
	}
	else if( offset_b == 12 && offset_g == 8  && offset_r == 4  && offset_a == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_BGRA4444;
	}
	else if( offset_r == 12 && offset_g == 8  && offset_b == 4  && offset_a == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_RGBA4444;
	}

	else if( offset_a == 15 && offset_b == 10 && offset_g == 5  && offset_r == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_ABGR1555;
	}
	else if( offset_a == 15 && offset_r == 10 && offset_g == 5  && offset_b == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_ARGB1555;
	}
	else if( offset_b == 11 && offset_g == 6  && offset_r == 1  && offset_a == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_BGRA5551;
	}
	else if( offset_r == 11 && offset_g == 6  && offset_b == 1  && offset_a == 0  && bpp == 16)
	{
		retval = TPL_FORMAT_RGBA5551;
	}

	else
	{
		TPL_WARN("Format not supported: offset_r=%d, offset_g=%d, offset_b=%d, offset_a=%d, bpp=%d",
				offset_r, offset_g, offset_b, offset_a, bpp);
	}

	return retval;
}
#endif

tpl_bool_t
__tpl_x11_display_get_window_info(tpl_display_t *display, tpl_handle_t window,
				       int *width, int *height, tpl_format_t *format, int depth, int a_size)
{
	TPL_IGNORE(depth);
	TPL_IGNORE(a_size);
	Status x_res;
	XWindowAttributes att;

	x_res = XGetWindowAttributes((Display *)display->native_handle, (Window)window, &att);

	if (x_res != BadWindow)
	{
		if (format != NULL)
		{
			switch (att.depth)
			{
				case 32: *format = TPL_FORMAT_ARGB8888; break;
				case 24: *format = TPL_FORMAT_XRGB8888; break;
				case 16: *format = TPL_FORMAT_RGB565; break;
				default: *format = TPL_FORMAT_INVALID; break;
			}
		}
		if (width != NULL) *width = att.width;
		if (height != NULL) *height = att.height;
		return TPL_TRUE;
	}

	return TPL_FALSE;

}

tpl_bool_t
__tpl_x11_display_get_pixmap_info(tpl_display_t *display, tpl_handle_t pixmap,
				       int *width, int *height, tpl_format_t *format)
{
	Status x_res;
	Window root = None;
	int x, y;
	unsigned int w, h, bw, d;

	x_res = XGetGeometry((Display *)display->native_handle, (Pixmap)pixmap, &root,
			     &x, &y, &w, &h, &bw, &d);

	if (x_res != BadDrawable)
	{
		if (format != NULL)
		{
			switch (d)
			{
				case 32: *format = TPL_FORMAT_ARGB8888; break;
				case 24: *format = TPL_FORMAT_XRGB8888; break;
				case 16: *format = TPL_FORMAT_RGB565; break;
				default: *format = TPL_FORMAT_INVALID; break;
			}
		}
		if (width != NULL)  *width = w;
		if (height != NULL) *height = h;
		if (format != NULL)
			*format = TPL_FORMAT_ARGB8888;/*TODO: temp for argb8888*/
		return TPL_TRUE;
	}

	return TPL_FALSE;
}

void
__tpl_x11_display_flush(tpl_display_t *display)
{
	Display *native_display = (Display *)display->native_handle;
	XFlush(native_display);
	XSync(native_display, False);
}


tpl_bool_t
__tpl_x11_buffer_init(tpl_buffer_t *buffer)
{
	TPL_IGNORE(buffer);
	return TPL_TRUE;
}

void
__tpl_x11_buffer_fini(tpl_buffer_t *buffer)
{
	if (buffer->backend.data)
	{
		tbm_bo_map((tbm_bo)buffer->backend.data, TBM_DEVICE_3D, TBM_OPTION_READ);
		tbm_bo_unmap((tbm_bo)buffer->backend.data);
		tbm_bo_unref((tbm_bo)buffer->backend.data);
		buffer->backend.data = NULL;
	}
}

void *
__tpl_x11_buffer_map(tpl_buffer_t *buffer, int size)
{
	tbm_bo bo;
	tbm_bo_handle handle;

	bo = (tbm_bo)buffer->backend.data;
	TPL_ASSERT(bo);

	handle = tbm_bo_get_handle(bo, TBM_DEVICE_CPU);
	return handle.ptr;
}

void
__tpl_x11_buffer_unmap(tpl_buffer_t *buffer, void *ptr, int size)
{
	TPL_IGNORE(buffer);
	TPL_IGNORE(ptr);
	TPL_IGNORE(size);

	/* Do nothing. */
}

tpl_bool_t
__tpl_x11_buffer_lock(tpl_buffer_t *buffer, tpl_lock_usage_t usage)
{
	tbm_bo bo;
	tbm_bo_handle handle;

	bo = (tbm_bo)buffer->backend.data;
	TPL_ASSERT(bo);

	TPL_OBJECT_UNLOCK(buffer);

	switch (usage)
	{
		case TPL_LOCK_USAGE_GPU_READ:
			handle = tbm_bo_map(bo, TBM_DEVICE_3D, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_GPU_WRITE:
			handle = tbm_bo_map(bo, TBM_DEVICE_3D, TBM_OPTION_WRITE);
			break;
		case TPL_LOCK_USAGE_CPU_READ:
			handle = tbm_bo_map(bo, TBM_DEVICE_CPU, TBM_OPTION_READ);
			break;
		case TPL_LOCK_USAGE_CPU_WRITE:
			handle = tbm_bo_map(bo, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
			break;
		default:
			TPL_ASSERT(TPL_FALSE);
			return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(buffer);

	if (handle.u32 != 0 || handle.ptr != NULL)
		return TPL_FALSE;

	return TPL_TRUE;
}

void
__tpl_x11_buffer_unlock(tpl_buffer_t *buffer)
{
	tbm_bo bo;

	bo = (tbm_bo)buffer->backend.data;
	TPL_ASSERT(bo);

	TPL_OBJECT_UNLOCK(buffer);
	tbm_bo_unmap(bo);
	TPL_OBJECT_LOCK(buffer);
}

tpl_bool_t __tpl_x11_buffer_get_reused_flag(tpl_buffer_t *buffer)
{
	if (DRI2_BUFFER_IS_REUSED(buffer->backend.flags))
		return TPL_TRUE;
	else
		return TPL_FALSE;
}

void __tpl_x11_display_wait_native(tpl_display_t *display)
{
    Display *xlib_display = NULL;
    xlib_display = (Display *)display->native_handle;
    if (xlib_display != NULL)
    {


		/* Leave events in the queue since we only care they have arrived. */
		XSync(xlib_display, 0);

    }
}
